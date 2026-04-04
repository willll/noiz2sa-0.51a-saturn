#pragma once
#include <srl.hpp>

using namespace SRL::Types;

namespace SRL
{
    class SMPC
    {
    private:
        /** @brief SMPC Commands
        */
        enum Commands : uint8_t
        {
            /** @brief Resets and enables the SH-2 Master CPU.
            */
            enableMasterCPU = 0x0,
            
            /** @brief Resets and enables the SH-2 Slave CPU.
            */
            enableSlaveCPU = 0x2,
            
            /** @brief Disables the SH-2 Slave CPU.
            */
            disableSlaveCPU = 0x3,
            
            /** @brief Resets and enables the Motorola C68K (sound) CPU.
            */
            enableSoundCPU = 0x6,
            
            /** @brief Disables the Motorola C68K (sound) CPU.
            */
            disableSoundCPU = 0x7,
            
            /** @brief Resets and enables the CD Block.
            */
            enableCD = 0x8,
            
            /** @brief Disables the CD Block.
            */
            disableCD = 0x9,
            
            /** @brief Resets and enables Netlink execution.
            */
            enableNetlink = 0xA,
            
            /** @brief Disables Netlink execution.
            */
            disableNetlink = 0xB,
            
            /** @brief 	Resets the System.
            */
            systemReset = 0xD,
            
            /** @brief Changes the system clockspeed
            */
            changeSystemClockSpeed352 = 0xE,
            
            /** @brief Changes the system clockspeed
            */
            changeSystemClockSpeed320 = 0xF,
            
            /** @brief Fetches the SMPC status and peripheral data.
            */
            fetchStatusAndPeripheralData = 0x10,
            
            /** @brief Sets the date and time for the RTC
            */
            setRTCDateAndTime = 0x16,
            
            /** @brief Sets the 4-byte battery-backed memory contained on the SMPC(which is used by the bios for language settings, etc.
            */
            setSMPCMemory = 0x17,
            
            /** @brief Sends an NMI request to the Master SH2
            */
            sendNMIRequestToMasterCPU = 0x18,
            
            /** @brief Enables NMI requests to be sent when the Reset button is pressed.
            */
            enableReset  = 0x19,
            
            /** @brief Disables NMI requests to be sent when the Reset button is pressed.
            */
            disableReset = 0x1A
        };

        /** @brief SMPC Memory-mapped I/O addresses
        */
        enum Address : uint32_t
        {
            /** @brief The command that's supposed to be issued by the SMPC
            */
            commandRegister = 0x2010001f,
            
            /** @brief Status Register
            */
            statusRegister = 0x20100061,
            
            /** @brief Status Flag. Shows the status of the SMPC command. Normally you set this to 1 when issuing out a command, and then the SMPC clears it when it's finished.
            */
            statusFlag = 0x20100063,
            
            /** @brief Input registers for command issuing. Whatever data the SMPC needs for processing command goes here.
            */
            inputRegister0 = 0x20100001,
            inputRegister1 = 0x20100003,
            inputRegister2 = 0x20100005,
            inputRegister3 = 0x20100007,
            inputRegister4 = 0x20100009,
            inputRegister5 = 0x2010000b,
            inputRegister6 = 0x2010000d,
            
            /** @brief Ouput Register for the command. If it supports it, it'll output any return data here
            */
            outputRegister0 = 0x20100021,
            outputRegister1 = 0x20100023,
            outputRegister2 = 0x20100025,
            outputRegister3 = 0x20100027,
            outputRegister4 = 0x20100029,
            outputRegister5 = 0x2010002b,
            outputRegister6 = 0x2010002d,
            outputRegister7 = 0x2010002f,
            outputRegister8 = 0x20100031,
            outputRegister9 = 0x20100033,
            outputRegister10 = 0x20100035,
            outputRegister11 = 0x20100037,
            outputRegister12 = 0x20100039,
            outputRegister13 = 0x2010003b,
            outputRegister14 = 0x2010003d,
            outputRegister15 = 0x2010003f,
            outputRegister16 = 0x20100041,
            outputRegister17 = 0x20100043,
            outputRegister18 = 0x20100045,
            outputRegister19 = 0x20100047,
            outputRegister20 = 0x20100049,
            outputRegister21 = 0x2010004b,
            outputRegister22 = 0x2010004d,
            outputRegister23 = 0x2010004f,
            outputRegister24 = 0x20100051,
            outputRegister25 = 0x20100053,
            outputRegister26 = 0x20100055,
            outputRegister27 = 0x20100057,
            outputRegister28 = 0x20100059,
            outputRegister29 = 0x2010005b,
            outputRegister30 = 0x2010005d,
            outputRegister31 = 0x2010005f,
            
            portDataRegister1 = 0x20100075,
            portDataRegister2 = 0x20100077,
            
            dataDirectionRegister1 = 0x20100079,
            dataDirectionRegister2 = 0x2010007b,
            
            inputOutputSelectRegister = 0x2010007d,
            
            externalLatchEnableRegister = 0x2010007f
        };
        
        static void writeByte(Address addr, uint8_t data)
        {
            *reinterpret_cast<volatile uint8_t *>(addr) = data;
        }

        static uint8_t readByte(Address addr)
        {
            return *reinterpret_cast<volatile uint8_t *>(addr);
        }

        static void wait()
        {
            while ((readByte(statusFlag) & 0x1) == 0x1) {
                // wait until the command is processed
            }
        }

        static void beginCommand()
        {
            wait();
            writeByte(statusFlag, 1);
        }

        static void endCommand(Commands cmd)
        {
            writeByte(commandRegister, cmd);
            wait();
        }
    public:      
        /** @brief Disables Sound CPU
        */
        static void DisableSoundCPU()
        {
            beginCommand();
            endCommand(disableSoundCPU);
        }

        /** @brief Enables Sound CPU
        */
        static void EnableSoundCPU()
        {
            beginCommand();
            endCommand(enableSoundCPU);
        }
        
        /** @brief Disables reset button
        */
        static void DisableReset()
        {
            beginCommand();
            endCommand(disableReset);
        }

        /** @brief Enables reset button
        */
        static void EnableReset()
        {
            beginCommand();
            endCommand(enableReset);
        }
    };
}

