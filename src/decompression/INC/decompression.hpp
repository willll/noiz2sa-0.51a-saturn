#pragma once

#include <srl.hpp>
#include <srl_log.hpp>

namespace SRL::Decompression
{    
	/**
	 * @brief RLE Decompression
	 * Very basic implentation
	 * @param Compressed data input
	 * @param Decompressed data output
	 * @param Decompressed length
	 */ 
	class Rle final
	{
	    public:
        static inline void Decompress(const uint8_t* input, uint8_t* output, size_t decompressedSize)
        {
            SRL::Logger::LogInfo("Rle::Decompress enter outSize=%u input=0x%08x output=0x%08x", (uint32_t)decompressedSize, (uint32_t)input, (uint32_t)output);
            size_t in  = 0;
            size_t out = 0;
            size_t nextTrace = decompressedSize >= 4 ? decompressedSize / 4 : decompressedSize;
            if (nextTrace == 0)
            {
                nextTrace = 1;
            }

            while (out < decompressedSize)
            {
                uint8_t run   = input[in++];
                uint8_t value = input[in++];

                for (uint8_t j = 0; j < run; j++)
                {
                    output[out++] = value;
                }

                if (out >= nextTrace)
                {
                    SRL::Logger::LogInfo("Rle::Decompress progress out=%u/%u in=%u", (uint32_t)out, (uint32_t)decompressedSize, (uint32_t)in);
                    nextTrace += decompressedSize >= 4 ? decompressedSize / 4 : 1;
                }
            }

            SRL::Logger::LogInfo("Rle::Decompress done out=%u in=%u", (uint32_t)out, (uint32_t)in);
        }
    };
    
    class Lzss final
    {
    public:
        /**
         * @brief LZSS Header
         * - String to detect LZSS compressed file
         * - Version Number
         * - Original File Type
         * - Original File Size
         * - Reserved / Future
         */
        struct Header
        {
            uint32_t magic;        // "LZSS"
            uint32_t version;
            uint32_t fileType;
            uint32_t originalSize;
            uint32_t reserved;
        };
        
        /**
         * @brief Compressed filetypes
         */
        enum FileTypes : uint32_t
        {
            Lz  = 0x4C5A5353, // `LZSS`
            Pcm = 0x2E50434D, // '.PCM'
            Snd = 0x2E534E44, // '.SND' (packed .PCM sound format)
            Tga = 0x2E544741, // `.TGA`
            Tm  = 0x2E544D00, // `.TM`  (packed VDP1 image format)
            Bin = 0x2E42494E  // `.BIN`
        };
        
        /**
         * @brief LZSS Decompression
         * Format:
         *  - Flags byte: 8 bits, MSB first
         *    1 = literal byte
         *    0 = (offset,length) pair (2 bytes)
         *  - Offset: 12 bits (1..4095)
         *  - Length: 4 bits + 3 (3..18)
         * @param Compressed data input
         * @param Decompressed data output
         * @param Decompressed length
         */
        static inline void Decompress(const uint8_t* input, uint8_t* output, size_t decompressedSize)
        {
            SRL::Logger::LogInfo("Lzss::Decompress enter outSize=%u input=0x%08x output=0x%08x", (uint32_t)decompressedSize, (uint32_t)input, (uint32_t)output);
            size_t inp  = 0;
            size_t outp = 0;

            uint8_t flags = 0;
            uint8_t mask  = 0;
            uint32_t flagGroups = 0;
            size_t nextTrace = decompressedSize >= 4 ? decompressedSize / 4 : decompressedSize;
            if (nextTrace == 0)
            {
                nextTrace = 1;
            }

            while (outp < decompressedSize)
            {
                if (mask == 0)
                {
                    flags = input[inp++];
                    mask  = 0x80; // MSB first
                    flagGroups++;

                    if ((flagGroups & 0x3F) == 0)
                    {
                        SRL::Logger::LogInfo("Lzss::Decompress flags group=%u in=%u out=%u", flagGroups, (uint32_t)inp, (uint32_t)outp);
                    }
                }

                if (flags & mask)
                {
                    // literal
                    if (outp >= decompressedSize)
                    {
                        SRL::Logger::LogFatal("Lzss::Decompress literal overflow out=%u size=%u in=%u", (uint32_t)outp, (uint32_t)decompressedSize, (uint32_t)inp);
                        return;
                    }

                    output[outp++] = input[inp++];
                }
                else
                {
                    // match
                    uint16_t pair = ((uint16_t)input[inp] << 8) | input[inp + 1];
                    inp += 2;

                    uint16_t encoded_offset = (pair >> 4) & 0x0FFF;
                    uint16_t length         = (pair & 0x000F) + 3;

                    uint16_t distance = encoded_offset + 1;
                    if (distance == 0 || distance > outp)
                    {
                        SRL::Logger::LogFatal(
                            "Lzss::Decompress invalid backref distance=%u out=%u in=%u",
                            (uint32_t)distance,
                            (uint32_t)outp,
                            (uint32_t)inp
                        );
                        return;
                    }

                    size_t src = outp - distance;

                    if (outp + length > decompressedSize)
                    {
                        uint16_t clamped = (uint16_t)(decompressedSize - outp);
                        SRL::Logger::LogWarning(
                            "Lzss::Decompress match clamp length=%u->%u out=%u size=%u",
                            (uint32_t)length,
                            (uint32_t)clamped,
                            (uint32_t)outp,
                            (uint32_t)decompressedSize
                        );
                        length = clamped;
                    }

                    // optional safety check (can remove for speed)
                    // if (distance == 0 || src >= outp) return;

                    for (uint16_t i = 0; i < length; i++)
                    {
                        output[outp++] = output[src + i];
                    }
                }

                mask >>= 1;

                if (outp >= nextTrace)
                {
                    SRL::Logger::LogInfo("Lzss::Decompress progress out=%u/%u in=%u", (uint32_t)outp, (uint32_t)decompressedSize, (uint32_t)inp);
                    nextTrace += decompressedSize >= 4 ? decompressedSize / 4 : 1;
                }
            }

            SRL::Logger::LogInfo("Lzss::Decompress done out=%u in=%u flagGroups=%u", (uint32_t)outp, (uint32_t)inp, flagGroups);
        }
    };
}
