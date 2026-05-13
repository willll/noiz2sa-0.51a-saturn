// TODO: check STN-9 https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/index.html?page=info/hon/stn09.htm
#pragma once
#include <smpc.hpp>

using namespace SRL::Math::Types;

namespace SRL::Backup {
    inline volatile uint32_t &BupLibAdress = *reinterpret_cast<volatile uint32_t *>(0x6000358);

    // Constants
    constexpr size_t MAX_FILENAME_LENGTH = 12; // must be 11 characters or less
    constexpr size_t MAX_COMMENT_LENGTH  = 11; // must be 10 characters or less
    constexpr size_t DEVICE_COUNT        = 3;
    constexpr size_t LIB_SPACE_SIZE      = 16384;
    constexpr size_t WORK_SPACE_SIZE     = 8312; // see STN-52 "Backup library work area size"

    // for BupDeviceConfig "id"
    enum Unit : uint8_t
    {
        Internal   = 1,
        Cartridge  = 2,
        Serial     = 3
    };

    // Device Config
    struct BupDeviceConfig
    {
        uint16_t id;
        uint16_t partition;
    };
    
    // Device Types
    enum BupDevice : uint8_t
    {
        InternalMemoryBackup = 0,
        CartridgeMemoryBackup,
        ExternalDeviceBackup // floppy drive
    };

    // Language types
    enum Language : uint8_t
    {
        Japanese   = 0,
        English,
        Francais,
        Deutsch,
        Espanol,
        Italiano
    };

    // Machine states (from BIOS)
    enum BupStatus : uint8_t
    {
        Success = 0,
        Failed,
        Unformatted,    // probably only applicable to FDD
        WriteProtected, // probably only applicable to FDD
        NotEnoughMemory,
        NotFound,
        Found,
        NoMatch,
        Broken
    };

    struct BupStatusTable
    {
        uint32_t TotalSize;
        uint32_t TotalBlocks;
        uint32_t BlockSize;
        uint32_t FreeSize;
        uint32_t FreeBlocks;
        uint32_t WritableCount;
    };

    // this is different from BupWriteTable, because it includes *Data
    struct BupFile
    {
        unsigned char Name[MAX_FILENAME_LENGTH] = {};
        unsigned char Comment[MAX_COMMENT_LENGTH] = {};
        void *Data = nullptr;
        uint32_t Datasize = 0;
        uint8_t  Language;
        // uint16_t BlockSize = 0; // from jo engine, probably not needed
    };

    struct BupDate {
        uint8_t year, month, day;
        uint8_t hour, minute, weekday;
    };

    // Device struct
    struct BupDeviceState {
        BupStatusTable StatTable{};
        int Status = -1;
        bool isMounted = false;
    };

    class Device
    {
    private:
        // Directory information table (needed for write)
        // not sure if this is ever used outside of core functions (see DirInfo)
        struct BupWriteTable
        {
            uint8_t	    filename[MAX_FILENAME_LENGTH];	/* File name */
            uint8_t	    comment[MAX_COMMENT_LENGTH];	/* Comments */
            uint8_t	    language;	    /* The language type of the comment */
            uint32_t    date;		    /* Time stamp */
            uint32_t	datasize;	    /* Data size (Byte) */
            uint16_t	blocksize;	    /* Data size (blocks) */
        };

        // Templates (see sega_bup.h)
        using BupInit = void (*)(volatile uint32_t* lib, uint32_t* work, BupDeviceConfig config[3]);
        using BupSelPart = int32_t (*)(uint32_t device, uint16_t partition);
        using BupFormat = int32_t (*)(uint32_t device);
        using BupStat = int32_t (*)(uint32_t device, uint32_t dummy, BupStatusTable *table);
        using BupWrite = int32_t (*)(uint32_t device, BupWriteTable *table, volatile uint8_t *data, bool overwrite);
        using BupExists = int32_t (*)(uint32_t device, uint8_t *filename, volatile uint8_t *dummy);
        using BupRead = int32_t (*)(uint32_t device, uint8_t *filename, volatile uint8_t *data);
        using BupDelete = int32_t (*)(uint32_t device, uint8_t *filename);
        using BupDir = int32_t (*)(uint32_t device, uint8_t *filename, uint16_t tbsize, BupWriteTable *table);
        using BupVerify = int32_t (*)(uint32_t device, uint8_t *filename, uint8_t *data);
        using BupGetDate = void (*)(uint32_t device, BupDate *table);
        using BupSetDate = int32_t (*)(BupDate *table);

        /**
         * @brief Resolves a BUP library function pointer from the in-RAM work area.
         * @tparam T     Function pointer type matching the target BUP routine.
         * @param offset Byte offset within the work area for the desired function entry.
         * @return Callable function pointer of type T.
         */
        template<typename T>
        static auto GetBupFunction(uint32_t offset)
        {
            auto ptr = reinterpret_cast<volatile uint8_t*>(WorkSpace) + offset;

            return reinterpret_cast<T>(
                *reinterpret_cast<volatile uint32_t*>(ptr)
            );
        }

        /**
         * @brief Loads the BUP driver from BIOS into HWRAM and initialises device configuration.
         * @param lib    Pointer to the library space buffer (LIB_SPACE_SIZE bytes in HWRAM).
         * @param work   Pointer to the work area buffer (WORK_SPACE_SIZE bytes in LWRAM).
         * @param config Array of three BupDeviceConfig entries, one per supported device.
         */
        static void DriverInit(uint32_t* lib, uint32_t* work, BupDeviceConfig* config) {
            reinterpret_cast<BupInit>(BupLibAdress)(lib, work, config);
        }

        /**
         * @brief Selects a partition on the given backup device.
         * @param device    The backup device to target.
         * @param partition Zero-based partition index to select.
         * @return BupStatus code indicating success or failure.
         */
        static int32_t SelectPartition(BupDevice device, uint16_t partition) {
            auto func = GetBupFunction<BupSelPart>(4);
            return func(device, partition);
        }

        /**
         * @brief Formats the given backup device, erasing all stored data.
         * @param device The backup device to format.
         * @return BupStatus code indicating success or failure.
         */
        static int32_t FormatDevice(BupDevice device) {
            auto func = GetBupFunction<BupFormat>(8);
            return func(device);
        }

        /**
         * @brief Queries the status and capacity information of a backup device.
         * @param device  The backup device to query.
         * @param outstat Output parameter filled with total/free size and block counts.
         * @return BupStatus code indicating device health.
         */
        static int32_t Status(BupDevice device, BupStatusTable* outstat)
        {
            auto func = GetBupFunction<BupStat>(12);
            return func(device, 1, outstat);
        }
        
        /**
         * @brief Writes a file to the backup device.
         * @param device    Target backup device.
         * @param file      Pointer to the BupFile describing the file name, comment, data, and size.
         * @param overwrite If true, an existing file with the same name is overwritten.
         * @return BupStatus code indicating success or failure.
         */
        static int32_t Write(BupDevice device, BupFile *file, bool overwrite)
        {
            BupWriteTable WriteTable = {};
           
            strcpy((char*)WriteTable.filename, (const char*)file->Name);
            strcpy((char*)WriteTable.comment, (const char*)file->Comment);
            
            // Jo engine calls SGL to get the SMPC language
            // (which is in a different order from BUP)
            // I don't want to do that.  The user can just set the language.
            WriteTable.language = file->Language;
                        
            // get date (check implementation in jo engine)
            // jo engine complicates this for no reason
            DateTime time = DateTime::Now();
            // doesn't currently work, need to figure out how to get SetDate to take SRL::Types::DateTime   
            // WriteTable.date = SetDate(time.ToBackupUnitDate());
            BupDate dateTable
            {
                static_cast<uint8_t>(time.Year() - 1980),
                time.Month(),
                time.Day(),
                time.Hour(),
                time.Minute(),
                time.Week()
            };
            WriteTable.date = SetDate(&dateTable);            
            
            WriteTable.datasize = file->Datasize; 
            // WriteTable.blocksize = file->Datasize * 64;  // not in sample so leave it out?
            
            // is this only needed for the FDD?
            // SelectPartition(device, 0);
            
            // because to Sega, true is false and false is true.
            overwrite = !overwrite;
            
            auto func = GetBupFunction<BupWrite>(16);
            return func(
                device,
                &WriteTable,
                static_cast<volatile unsigned char*>(file->Data),
                overwrite
            );
        }
        
        /**
         * @brief Retrieves the last-edit date/time from the backup device clock.
         * @param device The backup device to read the date from.
         * @param table  Output BupDate struct populated with year, month, day, hour, minute, weekday.
         */
        static void GetDate(BupDevice device, BupDate *table)
        {
            auto func = GetBupFunction<BupGetDate>(36);
            func(device, table);
        }

        /**
         * @brief Compresses a BupDate struct into a packed timestamp for the write table.
         * @param table Pointer to a BupDate to compress.
         * @return Packed timestamp value suitable for BupWriteTable::date.
         */
        static uint8_t SetDate(BupDate *table)
        {
            auto func = GetBupFunction<BupSetDate>(40);
            return func(table);
        }
        
        /**
         * @brief Allocates BUP library/work buffers and initialises the BUP driver.
         *        Idempotent — safe to call multiple times; returns immediately if already initialised.
         * @return true on successful initialisation, false if memory allocation failed.
         */
        static bool Init()
        {
            if (Initialized)
                return true;

            const bool hadLibrarySpace = (LibrarySpace != nullptr);

            if (!LibrarySpace)
            {
                LibrarySpace = static_cast<uint32_t*>(SRL::Memory::HighWorkRam::Malloc(LIB_SPACE_SIZE));
            }
            if (!LibrarySpace)
                return false;

            if (!WorkSpace)
            {
                WorkSpace = static_cast<uint32_t*>(SRL::Memory::LowWorkRam::Malloc(WORK_SPACE_SIZE));
            }
            if (!WorkSpace)
            {
                // Roll back only the allocation created in this call.
                if (!hadLibrarySpace && LibrarySpace)
                {
                    SRL::Memory::Free(LibrarySpace);
                    LibrarySpace = nullptr;
                }
                return false;
            }

            SRL::SMPC::DisableReset();

            DriverInit(LibrarySpace, WorkSpace, BupConfig);

            SRL::SMPC::EnableReset();

            for (auto& device : BupState)
                device.isMounted = false;

            Initialized = true;
            return true;
        }        
        
        /**
         * @brief Mounts a backup device and caches its status/capacity table.
         *        If the device is unformatted the mount fails without auto-formatting.
         * @param device The backup device to mount.
         * @return true if the device was mounted successfully, false otherwise.
         */
        static bool Mount(BupDevice device)
        {
            if (Initialized)
            {
                // instead of doing it here, have a "check for free space/blocks" function
                BupState[device].Status = Status(device, &BupState[device].StatTable);
                
                if (BupState[device].Status == Unformatted)
                {
                    // if (Format(device) == Failed)
                    // {
                        SRL::Debug::Print(2, 20, "Backup Device Not Formatted");
                        return false;
                    // }
                }

                BupState[device].isMounted = (BupState[device].Status == Success); // true or false

                return BupState[device].isMounted;
            }
            else
            {
                return false;
            }
        }

    public:
        static inline bool Initialized = false;
        static inline BupDeviceConfig BupConfig[DEVICE_COUNT] = {};
        static inline BupDeviceState BupState[DEVICE_COUNT];
        // only for debug
        static inline uint32_t* LibrarySpace = nullptr;
        static inline uint32_t* WorkSpace = nullptr;
        
        BupFile File;
        
        
        // zzz
        // work around - set filename to 13 chars.
        // next attempt:  use original Sega BUP library
        
        /**
         * @brief Constructs a Device, initialises the BUP driver, mounts available devices,
         *        and configures the embedded BupFile descriptor.
         * @param name     Null-terminated file name (max MAX_FILENAME_LENGTH-1 characters).
         * @param comment  Null-terminated comment string (max MAX_COMMENT_LENGTH-1 characters).
         * @param datasize Expected data payload size in bytes.
         * @param language Language code for the comment string (default: English).
         */
        Device
        (
            unsigned char* const name,
            unsigned char* const comment, 
            uint32_t datasize, 
            uint8_t language = English
        ) : File()
        {
            if (Init())
            {   
                // try to mount devices and get their status
                Mount(InternalMemoryBackup);
                Mount(CartridgeMemoryBackup);
                // Mount(ExternalDeviceBackup);
            }
            
            
            // Init BUP file
            // SRL::Debug::Print(1,22,"len=%d", strlen((char*)name));
            memcpy(File.Name, name, strlen((char*)name) + 1);
            memcpy(File.Comment, comment, strlen((char*)comment) + 1);
            // File.Data = nullptr;
            File.Datasize = datasize;
            File.Language = language;
        }
        
        /**
         * @brief Saves a data buffer to the backup device under this instance's file name.
         * @param device    Target backup device.
         * @param data      Pointer to the raw data to persist.
         * @param overwrite If true (default), an existing file with the same name is replaced.
         * @return BupStatus code, or 1 if the device is not mounted.
         */
        int32_t Save(BupDevice device, void* data, bool overwrite = true)
        {
            if (!BupState[device].isMounted)
                return 1;
            File.Data = data;
            SRL::SMPC::DisableReset();
            int32_t ret = Write(device, &File, overwrite);  // whats the status here?
            SRL::SMPC::EnableReset();
            return ret;
        }
        
        /**
         * @brief Checks whether a file with the given name exists on the backup device.
         * @param device   The backup device to search.
         * @param filename Null-terminated file name to look up.
         * @return BupStatus::Found if the file exists, BupStatus::NotFound otherwise.
         */
        int32_t FileExists(BupDevice device, unsigned char *filename)
        {
            auto func = GetBupFunction<BupExists>(20);
            return func(device, (uint8_t*)filename, reinterpret_cast<volatile uint8_t*>(1));
        }

        /**
         * @brief Reads a file from the backup device into a caller-supplied buffer.
         * @param device   The backup device to read from.
         * @param filename Null-terminated name of the file to read.
         * @param data     Caller-allocated buffer large enough to hold the file payload.
         * @return BupStatus code indicating success or failure.
         */
        int32_t Read(BupDevice device, unsigned char *filename, uint8_t *data)
        {
            auto func = GetBupFunction<BupRead>(20);
            // return func(device, (uint8_t*)filename, reinterpret_cast<volatile uint8_t*>(1));
            return func(device, (uint8_t*)filename, data);
        }
        
        
        /**
         * @brief Deletes a file from the backup device.
         * @param device   The backup device to delete from.
         * @param filename Null-terminated name of the file to delete.
         * @return BupStatus code indicating success or failure.
         */
        int32_t Delete(BupDevice device, unsigned char *filename)
        {
            
            auto func = GetBupFunction<BupDelete>(24);
            return func(device, (uint8_t*)filename);
        }
        
        
        /**
         * @brief Formats the given backup device if it is currently unformatted.
         * @param device The backup device to format.
         * @return true if the device was successfully formatted and mounted, false otherwise.
         */
        bool Format(BupDevice device)
        {
            if (Initialized)
            {
                BupState[device].Status = Status(device, &BupState[device].StatTable);
                
                if (BupState[device].Status == Unformatted)
                {
                    if (FormatDevice(device) == Failed)
                    {
                        SRL::Debug::Print(2, 20, "Backup Device Not Formatted");
                        return false;
                    }
                    else
                    {
                        BupState[device].isMounted = (BupState[device].Status == Success); // true or false
                        return BupState[device].isMounted;
                    }
                }
                else
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }

        /**
         * @brief Refreshes and returns the mount status of the given backup device.
         * @param device The backup device to check.
         * @return true if the device is successfully mounted, false otherwise.
         */
        static bool ReturnStatus(BupDevice device)
        {
            if (Initialized)
            {
                BupState[device].Status = Status(device, &BupState[device].StatTable);
                
                BupState[device].isMounted = (BupState[device].Status == Success); // true or false

                return BupState[device].isMounted;
            }
            else
            {
                return false;
            }
        }        

        /**
         * @brief Retrieves directory entry information for a file on the backup device.
         * @param device   The backup device to query.
         * @param filename Null-terminated file name to look up, or nullptr for all entries.
         * @return BupStatus code with directory information populated in the internal write table.
         */
        static int32_t DirInfo(BupDevice device, uint8_t *filename)
        {
            BupWriteTable WriteTable = {};
            uint16_t tbsize; // not sure what should be in here

            auto func = GetBupFunction<BupDir>(28);
            return func(device, filename, tbsize, &WriteTable);
        }

        /**
         * @brief Verifies the integrity of a file on the backup device by checksum comparison.
         * @param device   The backup device to verify against.
         * @param filename Null-terminated name of the file to verify.
         * @param data     Buffer containing the expected data to compare.
         * @return BupStatus code; Success if data matches, Broken if checksum fails.
         */
        static int32_t Verify(BupDevice device, uint8_t *filename, uint8_t *data)
        {
            auto func = GetBupFunction<BupVerify>(32);
            return func(device, filename, data);
        }

        /**
         * @brief Marks all devices as unmounted and resets the initialisation flag.
         *        Allocated library/work buffers are retained for reuse by future Device instances.
         */
        static void Unmount(void)
        {
            for (auto& device : BupState)
                device.isMounted = false;

            // Keep the allocated buffers so future Device instances can reuse
            // memory without extra HWRAM/LWRAM allocation churn.
            Initialized = false;
        }

        /**
         * @brief Frees the BUP library and work area buffers and fully resets driver state.
         *        After this call the driver must be re-initialised before any backup operation.
         */
        static void ReleaseBuffers(void)
        {
            if (LibrarySpace)
            {
                SRL::Memory::Free(LibrarySpace);
                LibrarySpace = nullptr;
            }
            if (WorkSpace)
            {
                SRL::Memory::Free(WorkSpace);
                WorkSpace = nullptr;
            }

            Initialized = false;
            for (auto& device : BupState)
                device.isMounted = false;
        }
        
        // need "delete" for object
    };
}