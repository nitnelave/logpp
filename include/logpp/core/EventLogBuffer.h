#pragma once

#include "logpp/core/Clock.h"
#include "logpp/core/FormatArgs.h"
#include "logpp/core/LogBuffer.h"
#include "logpp/core/LogFieldVisitor.h"
#include "logpp/core/SourceLocation.h"
#include "logpp/core/StringLiteral.h"

#include "logpp/utils/thread.h"
#include "logpp/utils/tuple.h"

#include <cstddef>
#include <optional>
#include <thread>

#include <fmt/format.h>

namespace logpp
{
    template <typename Key, typename Value, typename Enable = void>
    struct FieldWriter
    {
        FieldWriter() = delete;
    };

    template <typename Key, typename Value>
    static constexpr bool HasFieldWriter = std::is_constructible_v<FieldWriter<Key, Value>>;

    namespace details
    {
        template <typename T>
        using OffsetT = decltype(static_cast<LogBufferBase*>(nullptr)->write(std::declval<T>()));

        // Type-dependent for static_assert in if constexpr else branch
        template <typename...>
        constexpr std::false_type always_false {};

        template <typename Key, typename Value>
        struct DefaultFieldWriter
        {
            using ValueType = std::decay_t<Value>;

            static_assert(logpp::IsVisitable<ValueType>,
                          "Unsupported field value type. "
                          "If your field value provides an ostream<< operator, "
                          "you can include logpp/core/Ostream.h");

            template <typename Buffer>
            auto write(Buffer& buffer, const Key& key, const Value& value)
            {
                return std::make_pair(buffer.write(key), buffer.write(value));
            }
        };

        template <typename Arg>
        auto write(LogBufferBase& buffer, const Arg& arg)
        {
            using Key   = std::decay_t<typename Arg::Key>;
            using Value = std::decay_t<typename Arg::Type>;

            using Writer = std::conditional_t<
                IsVisitable<Value>,
                DefaultFieldWriter<Key, Value>,
                std::conditional_t<
                    HasFieldWriter<Key, Value>,
                    FieldWriter<Key, Value>,
                    DefaultFieldWriter<Key, Value>>>;

            auto [keyOffset, valueOffset] = Writer().write(buffer, arg.key, arg.value);
            return fieldOffset(keyOffset, valueOffset);
        }

        template <typename... Args>
        auto writeAll(LogBufferBase& buffer, Args&&... args)
        {
            return std::make_tuple(write(buffer, args)...);
        }

        template <typename Tuple, size_t... Indexes>
        auto writeFormatArgsImpl(LogBufferBase& buffer, const Tuple& args, std::index_sequence<Indexes...>)
        {
            return std::make_tuple(buffer.write(std::get<Indexes>(args))...);
        }

        template <typename... Args>
        auto writeFormatArgs(LogBufferBase& buffer, const std::tuple<Args...>& args)
        {
            return writeFormatArgsImpl(buffer, args, std::make_index_sequence<sizeof...(Args)> {});
        }

        template <typename>
        struct TextBlock;

        template <typename... Args>
        struct TextBlock<std::tuple<Args...>>
        {
            using ArgsOffsets = std::tuple<Args...>;

            TextBlock(StringOffset formatStrOffset, ArgsOffsets argsOffsets)
                : formatStrOffset { formatStrOffset }
                , argsOffsets { argsOffsets }
            { }

            void formatTo(fmt::memory_buffer& buffer, LogBufferView view) const
            {
                auto formatStr = formatStrOffset.get(view);
                if constexpr (sizeof...(Args) == 0)
                    buffer.append(formatStr);
                else
                    formatImpl(view, buffer, formatStr, std::make_index_sequence<sizeof...(Args)> {});
            }

        private:
            StringOffset formatStrOffset;
            ArgsOffsets argsOffsets;

            template <size_t... Indexes>
            void formatImpl(LogBufferView view, fmt::memory_buffer& buffer, std::string_view formatStr, std::index_sequence<Indexes...>) const
            {
                fmt::format_to(std::back_inserter(buffer), fmt::runtime(formatStr), getArg<Indexes>(view)...);
            }

            template <size_t N>
            auto getArg(LogBufferView view) const
            {
                auto offset = std::get<N>(argsOffsets);
                return offset.get(view);
            }
        };

        template <typename>
        struct FieldsBlock;

#pragma pack(push, 1)
        template <typename... Fields>
        struct FieldsBlock<std::tuple<Fields...>>
        {
            using Offsets = std::tuple<Fields...>;

            FieldsBlock(Offsets offsets)
                : offsets { offsets }
            { }

            void visit(LogBufferView view, LogFieldVisitor& visitor) const
            {
                tuple_utils::visit(offsets, [&](const auto& fieldOffset) {
                    auto key   = fieldOffset.key.get(view);
                    auto value = fieldOffset.value.get(view);
                    visitor.visit(key, value);
                });
            }

            constexpr size_t count() const
            {
                return sizeof...(Fields);
            }

        private:
            Offsets offsets;
        };
#pragma pack(pop)

        template <typename Offsets>
        FieldsBlock<Offsets> fieldsBlock(Offsets offsets)
        {
            return { offsets };
        }

        template <typename ArgsOffsets>
        TextBlock<ArgsOffsets> textBlock(StringOffset textOffset, ArgsOffsets argsOffsets)
        {
            return { textOffset, argsOffsets };
        }

        struct SourceLocationBlock
        {
            StringLiteralOffset file;
            Offset<size_t> line;
        };
    }

    template <typename KeyOffset, typename OffsetT>
    struct FieldOffset
    {
        KeyOffset key;
        OffsetT value;
    };

    template <typename KeyOffset, typename OffsetT>
    FieldOffset<KeyOffset, OffsetT> fieldOffset(KeyOffset key, OffsetT value)
    {
        return { key, value };
    }

    class EventLogBuffer : public LogBuffer<256>
    {
    public:
        using TextFormatFunc  = void (*)(const LogBufferBase& buffer, OffsetType offsetsIndex, fmt::memory_buffer& formatBuf);
        using FieldsVisitFunc = void (*)(const LogBufferBase& buffer, OffsetType offsetsIndex, LogFieldVisitor& visitor);

        static constexpr size_t HeaderOffset = 0;

        struct Header
        {
            TimePoint timePoint;
            thread_utils::id threadId;

            OffsetType textBlockIndex;
            TextFormatFunc formatFunc;

            OffsetType sourceLocationBlockIndex;

            uint8_t fieldsCount;
            OffsetType fieldsBlockIndex;
            FieldsVisitFunc fieldsVisitFunc;
        };

        EventLogBuffer()
        {
            auto* header                     = decodeHeader();
            header->sourceLocationBlockIndex = 0;
            header->fieldsCount              = 0;
            advance(HeaderOffset + sizeof(Header));
        }

        void writeTime(TimePoint timePoint)
        {
            decodeHeader()->timePoint = timePoint;
        }

        void writeThreadId(thread_utils::id threadId)
        {
            decodeHeader()->threadId = threadId;
        }

        template <typename Str>
        void writeText(const Str& str)
        {
            auto offset = this->write(str);

            auto block       = details::textBlock(offset, std::make_tuple());
            auto blockOffset = this->encode(block);

            encodeTextBlock(block, blockOffset);
        }

        template <typename... Args>
        void writeText(const FormatArgsHolder<Args...>& holder)
        {
            auto formatStrOffset = this->write(holder.formatStr);
            auto argsOffsets     = details::writeFormatArgs(*this, holder.args);

            auto block       = details::textBlock(formatStrOffset, argsOffsets);
            auto blockOffset = this->encode(block);

            encodeTextBlock(block, blockOffset);
        }

        void writeSourceLocation(const SourceLocation& location)
        {
            auto fileOffset = this->write(StringLiteral { location.file.data() });
            auto lineOffset = this->write(location.line);

            details::SourceLocationBlock block { fileOffset, lineOffset };
            auto blockOffset = this->encode(block);

            auto* header                     = overlayAt<Header>(HeaderOffset);
            header->sourceLocationBlockIndex = static_cast<OffsetType>(blockOffset);
        }

        template <typename... Fields>
        void writeFields(Fields&&... fields)
        {
            auto offsets = details::writeAll(*this, std::forward<Fields>(fields)...);

            auto block       = details::fieldsBlock(offsets);
            auto blockOffset = this->encode(block);

            encodeFieldsBlock(block, blockOffset);
        }

        void visitFields(LogFieldVisitor& visitor) const
        {
            const auto* header = decodeHeader();
            if (header->fieldsCount > 0)
                std::invoke(header->fieldsVisitFunc, *this, header->fieldsBlockIndex, visitor);
        }

        TimePoint time() const
        {
            return decodeHeader()->timePoint;
        }

        thread_utils::id threadId() const
        {
            return decodeHeader()->threadId;
        }

        void formatText(fmt::memory_buffer& buffer) const
        {
            const auto* header = decodeHeader();
            std::invoke(header->formatFunc, *this, header->textBlockIndex, buffer);
        }

        std::optional<SourceLocation> location() const
        {
            const auto* header = decodeHeader();
            auto index         = header->sourceLocationBlockIndex;
            if (index == 0)
                return std::nullopt;

            LogBufferView view { *this };
            const auto* block = view.overlayAs<details::SourceLocationBlock>(index);

            return SourceLocation {
                block->file.get(view),
                block->line.get(view)
            };
        }

    private:
        template <typename Block>
        void encodeTextBlock(const Block&, size_t blockIndex)
        {
            auto* header           = overlayAt<Header>(HeaderOffset);
            header->textBlockIndex = static_cast<OffsetType>(blockIndex);
            header->formatFunc     = [](const LogBufferBase& buffer, OffsetType blockIndex, fmt::memory_buffer& formatBuf) {
                LogBufferView view { buffer };
                const Block* block = view.overlayAs<Block>(blockIndex);
                block->formatTo(formatBuf, view);
            };
        }

        template <typename Block>
        void encodeFieldsBlock(const Block& block, size_t blockIndex)
        {
            auto* header             = overlayAt<Header>(HeaderOffset);
            header->fieldsCount      = static_cast<uint8_t>(block.count());
            header->fieldsBlockIndex = static_cast<uint32_t>(blockIndex);
            header->fieldsVisitFunc  = [](const LogBufferBase& buffer, OffsetType blockIndex, LogFieldVisitor& visitor) {
                LogBufferView view { buffer };
                const Block* block = view.overlayAs<Block>(blockIndex);

                visitor.visitStart(block->count());
                block->visit(view, visitor);
                visitor.visitEnd();
            };
        }

        Header* decodeHeader()
        {
            return overlayAt<Header>(HeaderOffset);
        }

        const Header* decodeHeader() const
        {
            return overlayAt<Header>(HeaderOffset);
        }
    };
}
