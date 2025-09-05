#include <iostream>
#include <cstdint>
#include <fstream>
#include <iomanip>

#include <vector>
#include <string>
#include <thread>
#include <chrono>

/* 
Sources:
    - https://ru.wikipedia.org/wiki/MOS_Technology_6502
    - https://www.masswerk.at/6502/6502_instruction_set.html
    - https://www.masswerk.at/6502/assembler.html
*/

uint8_t memory[0xffff];

class Peripheral {
    public:
        std::string name;
        
        uint16_t start;
        
        virtual void write(uint8_t address, uint8_t value) = 0;
        virtual uint8_t read(uint8_t address) = 0;
        virtual void run() = 0;
};

// class PeripheralA : public Peripheral {
//     private:
//         uint8_t regA = 0;
//         uint8_t regB = 0;
//         uint8_t regC = 0;
    
//         bool wrongAddress = false;

//         std::thread runThread;
//     public:
//         PeripheralA() {
//             name = "PeripheralA";
//             start = 0x3ff;

//             runThread = std::thread(&Peripheral::run, this);
//         }

//         void write(uint8_t address, uint8_t value) {
//             wrongAddress = false;
//             if (address == 0) regA = value;
//             else if (address == 1) regB = value;
//             else if (address == 2) regC = value;
//             else wrongAddress = true;
//         }

//         uint8_t read(uint8_t address) {
//             wrongAddress = false;
//             if (address == 0) return regA;
//             else if (address == 1) return regB;
//             else if (address == 2) return regC;
//             else {
//                 wrongAddress = true;
//                 return 0;
//             }
//         }

//         void run() {
//             while (true) {
//                 std::this_thread::sleep_for(std::chrono::seconds(1));
//                 std::cout << "regA = " << regA << std::endl;
//                 std::cout << "regB = " << regB << std::endl;
//                 std::cout << "regC = " << regC << std::endl;
//             }
//         }

//         ~PeripheralA() {
//             runThread.join();
//         }
// };

// List of peripherals
std::vector<Peripheral*> peripherals;

void write(uint16_t address, uint8_t value) {
    for (auto *peripheral: peripherals) {
        if (address >= peripheral->start && address <= peripheral->start + 0xff) {
            peripheral->write((uint8_t)address, value);
            return;
        }
    }

    memory[address] = value;
}

uint8_t read(uint16_t address) {
    for (auto *peripheral: peripherals) {
        if (address >= peripheral->start && address <= peripheral->start + 0xff) {
            return peripheral->read((uint8_t)address);
        }
    }

    return memory[address];
}

enum {
    CARRY_FLAG = 0x1,
    ZERO_FLAG = 0x2,
    INTERRUPT_FLAG = 0x4,
    DECIMAL_FLAG = 0x8,
    BREAK_FLAG = 0xf,
    OVERFLOW_FLAG = 0x40,
    NEGATIVE_FLAG = 0x80
}; 

enum {
    IRQ = 0xffff,
    RESET = 0xfffd,
    NMI = 0xfffb
};

class CPU {
    bool isIRQ = false;
    bool isNMI = false;

    bool debug = false;

    public:
        uint8_t accumulator = 0;
        uint8_t x = 0, y = 0;
        uint8_t sp = 0;
        uint8_t psr = 0;
        
        uint8_t instr_reg = 0;

        uint16_t pc = 0;

        CPU(bool isDebug=false) {
            debug = isDebug;
        }

        void pushStack(uint8_t data) {
            write(((uint16_t)0x01 << 8) | sp, data);
            sp--;
        }

        uint8_t pullStack() {
            sp++;
            return read(((uint16_t)0x01 << 8) | sp);
        }

        void pushPC() {
            uint8_t high = pc >> 8;
            uint8_t low = pc;

            pushStack(high);
            pushStack(low);
        }

        uint16_t pullPC() {
            uint8_t low = pullStack();
            uint8_t high = pullStack();

            return ((uint16_t)high << 8) | low;
        }

        void reset() {
            pc = ((uint16_t)read(RESET) << 8) | read(RESET-1);
        }

        uint16_t absoluteAddress() {
            return ((uint16_t)read(pc+1) << 8) | read(pc+2);
        }

        uint8_t immediateValue() {
            return read(pc + 1);
        }

        uint8_t relativeValue() {
            return read(pc + 1);
        }

        uint16_t absoluteIndexedY() {
            return absoluteAddress()+y;
        }

        uint16_t absoluteIndexedX() {
            return absoluteAddress()+x;
        }
        
        uint16_t indirectAbsoluteAddress() {
            uint8_t low = read(absoluteAddress());
            uint16_t high = read(absoluteAddress()+1) << 8;
            return high | low;
        }

        uint16_t indirectIndexedAddress() {
            uint8_t low = read(pc+1);
            uint8_t high = read(pc+2);
            uint16_t address = ((uint16_t)high << 8) | low;
            return address+y;
        }

        uint16_t indexedIndirectAddress() {
            uint8_t zeroPage = read(pc+1) + x;
            uint8_t low = read(zeroPage);
            uint8_t high = read(zeroPage+1);

            return ((uint16_t)high << 8) | low;
        }

        uint16_t zeroPagedIndexedXAddress() {
            return read(pc+1)+x;
        }

        uint16_t zeroPagedIndexedYAddress() {
            return read(pc+1)+y;
        }

        uint8_t zeroPagedAddress() {
            return read(pc+1);
        }
        
        void executeIRQ() {
            pushPC();
            pushStack(psr);

            pc = ((uint16_t)read(IRQ) << 8) | read(IRQ-1);

            setFlag(INTERRUPT_FLAG);

            isIRQ = false;
        }

        void executeNMI() {
            pushPC();
            pushStack(psr);

            pc = ((uint16_t)read(NMI) << 8) | read(NMI-1);

            setFlag(INTERRUPT_FLAG);

            isNMI = false;
        }

        void setFlag(uint8_t flag) {
            psr &= flag;
        }

        void unsetFlag(uint8_t flag) {
            psr &= ~flag;
        }

        bool checkFlag(uint8_t flag) {
            return ((psr & flag) > 0);
        }

        void run() {
            reset();
            
            while (true) {
                if (isIRQ && !checkFlag(INTERRUPT_FLAG)) {
                    executeIRQ();
                } else if (isNMI) {
                    executeNMI();
                } 
                 
                instr_reg = read(pc);

                if (!decode()) return;
            }
        }

        bool decode() {
            switch (instr_reg) {
                case 0x00:
                    if (debug) std::cout << "BRK" << std::endl;
                    BRK();
                    break;
                case 0x01:
                    if (debug) std::cout << "ORA X, ind" << std::endl;
                    ORA(read(indirectIndexedAddress()));
                    pc += 2;
                    break;
                case 0x05:
                    if (debug) std::cout << "ORA zpg" << std::endl;
                    ORA(read(zeroPagedAddress()));
                    pc += 2;
                    break;
                case 0x06:
                    if (debug) std::cout << "ASL zpg" << std::endl;
                    ASL(read(zeroPagedAddress()));
                    pc += 2;
                    break;
                case 0x08:
                    if (debug) std::cout << "PHP" << std::endl;
                    PHP();
                    pc += 1;
                    break;
                case 0x09:
                    if (debug) std::cout << "ORA #" << std::endl;
                    ORA(immediateValue());
                    pc += 2;
                    break;
                case 0x0a:
                    if (debug) std::cout << "ASL A" << std::endl;
                    ASL();
                    pc += 1;
                    break;
                case 0x0d:
                    if (debug) std::cout << "ORA abs" << std::endl;
                    ORA(read(absoluteAddress()));
                    pc += 3;
                    break;
                case 0x0e:
                    if (debug) std::cout << "ASL abs" << std::endl;
                    ASL(absoluteAddress());
                    pc += 3;
                    break;

                case 0x10:
                    if (debug) std::cout << "BPL rel" << std::endl;
                    BPL(relativeValue());
                    break;
                case 0x11:
                    if (debug) std::cout << "ORA ind, Y" << std::endl;
                    ORA(read(indirectIndexedAddress()));
                    pc += 2;
                    break;
                case 0x15:
                    if (debug) std::cout << "ORA zpg, X" << std::endl;
                    ORA(read(zeroPagedIndexedXAddress()));
                    pc += 2;
                    break;
                case 0x16:
                    if (debug) std::cout << "ASL zpg, X" << std::endl;
                    ASL(zeroPagedIndexedXAddress());
                    pc += 2;
                    break;
                case 0x18:
                    if (debug) std::cout << "CLC" << std::endl;
                    CLC();
                    pc += 1;
                    break;
                case 0x19:
                    if (debug) std::cout << "ORA abs, Y" << std::endl;
                    ORA(read(absoluteIndexedY()));
                    pc += 3;
                    break;
                case 0x1d:
                    if (debug) std::cout << "ORA abs, X" << std::endl;
                    ORA(read(absoluteIndexedX()));
                    pc += 3;
                    break;
                case 0x1e:
                    if (debug) std::cout << "ASL abs, X" << std::endl;
                    ASL(absoluteIndexedX());
                    pc += 3;
                    break;

                case 0x20:
                    if (debug) std::cout << "JSR abs" << std::endl;
                    JSR(absoluteAddress());
                    break;
                case 0x21:
                    if (debug) std::cout << "AND X, ind" << std::endl;
                    AND(read(indirectIndexedAddress()));
                    pc += 2;
                    break;
                case 0x24:
                    if (debug) std::cout << "BIT zpg" << std::endl;
                    BIT(read(zeroPagedAddress()));
                    pc += 2;
                    break;
                case 0x25:
                    if (debug) std::cout << "AND zpg" << std::endl;
                    AND(read(zeroPagedIndexedXAddress()));
                    pc += 2;
                    break;
                case 0x26:
                    if (debug) std::cout << "ROL zpg" << std::endl;
                    ROL(zeroPagedAddress());
                    pc += 2;
                    break;
                case 0x28:
                    if (debug) std::cout << "PLP" << std::endl;
                    PLP();
                    pc += 1;
                    break;
                case 0x29:
                    if (debug) std::cout << "AND #" << std::endl;
                    AND(immediateValue());
                    pc += 1;
                    break;
                case 0x2a:
                    if (debug) std::cout << "ROL A" << std::endl;
                    ROL();
                    pc += 1;
                    break;
                case 0x2c:
                    if (debug) std::cout << "BIT abs" << std::endl;
                    BIT(read(absoluteAddress()));
                    pc += 3;
                    break;
                case 0x2d:
                    if (debug) std::cout << "AND abs" << std::endl;
                    AND(read(absoluteAddress()));
                    pc += 3;
                    break;
                case 0x2e:
                    if (debug) std::cout << "ROL abs" << std::endl;
                    ROL(absoluteAddress());
                    pc += 3;
                    break;

                case 0x30:
                    if (debug) std::cout << "BMI rel" << std::endl;
                    BMI(relativeValue());
                    break;
                case 0x31:
                    if (debug) std::cout << "AND ind, Y" << std::endl;
                    AND(read(indirectIndexedAddress()));
                    pc += 2;
                    break;
                case 0x35:
                    if (debug) std::cout << "AND zpg, X" << std::endl;
                    AND(read(zeroPagedIndexedXAddress()));
                    pc += 2;
                    break;
                case 0x36:
                    if (debug) std::cout << "ROL zpg, X" << std::endl;
                    ROL(zeroPagedAddress());
                    pc += 2;
                    break;
                case 0x38:
                    if (debug) std::cout << "SEC" << std::endl;
                    SEC();
                    pc += 1;
                    break;
                case 0x39:
                    if (debug) std::cout << "AND abs, Y" << std::endl;
                    AND(read(absoluteIndexedY()));
                    pc += 3;
                    break;
                case 0x3d:
                    if (debug) std::cout << "AND abs, X" << std::endl;
                    AND(read(absoluteIndexedX()));
                    pc += 3;
                    break;
                case 0x3e:
                    if (debug) std::cout << "ROL abs, X" << std::endl;
                    ROL(read(absoluteIndexedX()));
                    pc += 3;
                    break;

                case 0x40:
                    if (debug) std::cout << "RTI" << std::endl;
                    RTI();
                    break;
                case 0x41:
                    if (debug) std::cout << "EOR X, ind" << std::endl;
                    EOR(read(indexedIndirectAddress()));
                    pc += 2;
                    break;
                case 0x45:
                    if (debug) std::cout << "EOR zpg" << std::endl;
                    EOR(read(zeroPagedAddress()));
                    pc += 2;
                    break;
                case 0x46:
                    if (debug) std::cout << "LSR zpg" << std::endl;
                    LSR(zeroPagedAddress());
                    pc += 2;
                    break;
                case 0x48:
                    if (debug) std::cout << "PHA" << std::endl;
                    PHA();
                    pc += 1;
                    break;
                case 0x49:
                    if (debug) std::cout << "EOR #" << std::endl;
                    EOR(immediateValue());
                    pc += 2;
                    break;
                case 0x4a:
                    if (debug) std::cout << "LSR A" << std::endl;
                    LSR();
                    pc += 1;
                    break;
                case 0x4c:
                    if (debug) std::cout << "JMP abs" << std::endl;
                    JMP(absoluteAddress());
                    break;
                case 0x4d:
                    if (debug) std::cout << "EOR abs" << std::endl;
                    EOR(read(absoluteAddress()));
                    pc += 3;
                    break;
                case 0x4e:
                    if (debug) std::cout << "LSR abs" << std::endl;
                    LSR(absoluteAddress());
                    pc += 3;
                    break;
                
                case 0x50:
                    if (debug) std::cout << "BVC rel" << std::endl;
                    BVC(relativeValue());
                    break;
                case 0x51:
                    if (debug) std::cout << "EOR ind, Y" << std::endl;
                    EOR(read(indirectIndexedAddress()));
                    pc += 2;
                    break;
                case 0x55:
                    if (debug) std::cout << "EOR zpg, X" << std::endl;
                    EOR(read(zeroPagedIndexedXAddress()));
                    pc += 2;
                    break;
                case 0x56:
                    if (debug) std::cout << "LSR zpg, X" << std::endl;
                    LSR(zeroPagedIndexedXAddress());
                    pc += 2;
                    break;
                case 0x58:
                    if (debug) std::cout << "CLI" << std::endl;
                    CLI();
                    pc += 1;
                    break;
                case 0x59:
                    if (debug) std::cout << "EOR abs, Y" << std::endl;
                    EOR(read(absoluteIndexedY()));
                    pc += 3;
                    break;
                case 0x5d:
                    if (debug) std::cout << "EOR abs, X" << std::endl;
                    EOR(read(absoluteIndexedX()));
                    pc += 3;
                    break;
                case 0x5e:
                    if (debug) std::cout << "LSR abs, X" << std::endl;
                    LSR(absoluteIndexedX());
                    pc += 3;
                    break;
                
                case 0x60:
                    if (debug) std::cout << "RTS" << std::endl;
                    RTS();
                    break;
                case 0x61:
                    if (debug) std::cout << "ADC X, ind" << std::endl;
                    ADC(read(indexedIndirectAddress()));
                    pc += 2;
                    break;
                case 0x65:
                    if (debug) std::cout << "ADC zpg" << std::endl;
                    ADC(read(zeroPagedAddress()));
                    pc += 2;
                    break;
                case 0x66:
                    if (debug) std::cout << "ROR zpg" << std::endl;
                    ROR(zeroPagedAddress());
                    pc += 2;
                    break;
                case 0x68:
                    if (debug) std::cout << "PLA" << std::endl;
                    PLA();
                    pc += 2;
                    break;
                case 0x69:
                    if (debug) std::cout << "ADC #" << std::endl;
                    ADC(immediateValue());
                    pc += 2;
                    break;
                case 0x6a:
                    if (debug) std::cout << "ROR A" << std::endl;
                    ROR();
                    pc += 2;
                    break;
                case 0x6c:
                    if (debug) std::cout << "JMP ind" << std::endl;
                    JMP(indirectAbsoluteAddress());
                    break;
                case 0x6d:
                    if (debug) std::cout << "ADC abs" << std::endl;
                    ADC(read(absoluteAddress()));
                    pc += 3;
                    break;
                case 0x6e:
                    if (debug) std::cout << "ROR abs" << std::endl;
                    ROR(absoluteAddress());
                    pc += 3;
                    break;
                
                case 0x70:
                    if (debug) std::cout << "BVS rel" << std::endl;
                    BVS(relativeValue());
                    pc += 2;
                    break;
                case 0x71:
                    if (debug) std::cout << "ADC ind, Y" << std::endl;
                    ADC(read(indirectIndexedAddress()));
                    pc += 2;
                    break;                
                case 0x75:
                    if (debug) std::cout << "ADC zpg, X" << std::endl;
                    ADC(read(zeroPagedIndexedXAddress()));
                    pc += 2;
                    break;
                case 0x76:
                    if (debug) std::cout << "ROR zpg, X" << std::endl;
                    ROR(zeroPagedIndexedXAddress());
                    pc += 2;
                    break;
                case 0x7d:
                    if (debug) std::cout << "ADC abs, X" << std::endl;
                    ADC(read(absoluteIndexedX()));
                    pc += 3;
                    break;
                case 0x7e:
                    if (debug) std::cout << "ROR abs, X" << std::endl;
                    ROR(read(absoluteIndexedX()));
                    pc += 3;
                    break;

                case 0x81:
                    if (debug) std::cout << "STA X, ind" << std::endl;
                    STA(indexedIndirectAddress());
                    pc += 2;
                    break;
                case 0x84:
                    if (debug) std::cout << "STY zpg" << std::endl;
                    STY(zeroPagedAddress());
                    pc += 2;
                    break;
                case 0x85:
                    if (debug) std::cout << "STA zpg" << std::endl;
                    STA(zeroPagedAddress());
                    pc += 2;
                    break;
                case 0x86:
                    if (debug) std::cout << "STX, zpg" << std::endl;
                    STX(zeroPagedAddress());
                    pc += 2;
                    break;
                case 0x88:
                    if (debug) std::cout << "DEY" << std::endl;
                    DEY();
                    pc += 1;
                    break;
                case 0x8a:
                    if (debug) std::cout << "TXA" << std::endl;
                    TXA();
                    pc += 1;
                    break;
                case 0x8c:
                    if (debug) std::cout << "STY abs" << std::endl;
                    STY(absoluteAddress());
                    pc += 3;
                    break;
                case 0x8d:
                    if (debug) std::cout << "STA abs" << std::endl;
                    STA(absoluteAddress());
                    pc += 3;
                    break;
                case 0x8e:
                    if (debug) std::cout << "STX abs" << std::endl;
                    STX(absoluteAddress());
                    pc += 3;
                    break;

                case 0x90:
                    if (debug) std::cout << "BCC rel" << std::endl;
                    BCC(relativeValue());
                    pc += 2;
                    break;
                case 0x91:
                    if (debug) std::cout << "STA X, ind" << std::endl;
                    STA(indexedIndirectAddress());
                    pc += 2;
                    break;
                case 0x94:
                    if (debug) std::cout << "STY zpg, X" << std::endl;
                    STY(zeroPagedIndexedXAddress());
                    pc += 2;
                    break;
                case 0x95:
                    if (debug) std::cout << "STA zpg, X" << std::endl;
                    STA(zeroPagedIndexedXAddress());
                    pc += 2;
                    break;
                case 0x96:
                    if (debug) std::cout << "STX zpg, Y" << std::endl;
                    STX(zeroPagedIndexedYAddress());
                    pc += 2;
                    break;
                case 0x98:
                    if (debug) std::cout << "TYA" << std::endl;
                    TYA();
                    pc += 1;
                    break;
                case 0x99:
                    if (debug) std::cout << "STA abs, Y" << std::endl;
                    STA(absoluteIndexedY());
                    pc += 3;
                    break;
                case 0x9a:
                    if (debug) std::cout << "TXS" << std::endl;
                    TXS();
                    pc += 1;
                    break;
                case 0x9d:
                    if (debug) std::cout << "STA abs, X" << std::endl;
                    STA(absoluteIndexedX());
                    pc += 3;
                    break;
                
                case 0xa0:
                    if (debug) std::cout << "LDY #" << std::endl;
                    LDY(immediateValue());
                    pc += 2;
                    break;
                case 0xa1:
                    if (debug) std::cout << "LDA X, ind" << std::endl;
                    LDA(read(indexedIndirectAddress()));
                    pc += 2;
                    break;
                case 0xa2:
                    if (debug) std::cout << "LDX #" << std::endl;
                    LDX(immediateValue());
                    pc += 2;
                    break;
                case 0xa4:
                    if (debug) std::cout << "LDY zpg" << std::endl;
                    LDY(read(zeroPagedAddress()));
                    pc += 2;
                    break;
                case 0xa5:
                    if (debug) std::cout << "LDA zpg" << std::endl;
                    LDA(read(zeroPagedAddress()));
                    pc += 2;
                    break;
                case 0xa6:
                    if (debug) std::cout << "LDX zpg" << std::endl;
                    LDX(read(zeroPagedAddress()));
                    pc += 2;
                    break;
                case 0xa8:
                    if (debug) std::cout << "TAY" << std::endl;
                    TAY();
                    pc += 1;
                    break;
                case 0xa9:
                    if (debug) std::cout << "LDA #" << std::endl;
                    LDA(immediateValue());
                    pc += 2;
                    break;
                case 0xaa:
                    if (debug) std::cout << "TAX" << std::endl;
                    TAX();
                    pc += 1;
                    break;
                case 0xac:
                    if (debug) std::cout << "LDY abs" << std::endl;
                    LDY(read(absoluteAddress()));
                    pc += 3;
                    break;
                case 0xad:
                    if (debug) std::cout << "LDA abs" << std::endl;
                    LDA(read(absoluteAddress()));
                    pc += 3;
                    break;
                case 0xae:
                    if (debug) std::cout << "LDX abs" << std::endl;
                    LDX(read(absoluteAddress()));
                    pc += 3;
                    break;

                case 0xb0:
                    if (debug) std::cout << "BCS rel" << std::endl;
                    BCS(relativeValue());
                    pc += 2;
                    break;
                case 0xb1:
                    if (debug) std::cout << "LDA ind, Y" << std::endl;
                    LDA(read(indirectIndexedAddress()));
                    pc += 2;
                    break;
                case 0xb4:
                    if (debug) std::cout << "LDY zpg, X" << std::endl;
                    LDY(read(zeroPagedIndexedXAddress()));
                    pc += 2;
                    break;
                case 0xb5:
                    if (debug) std::cout << "LDA zpg, X" << std::endl;
                    LDA(read(zeroPagedIndexedXAddress()));
                    pc += 2;
                    break;
                case 0xb6:
                    if (debug) std::cout << "LDX zpg, X" << std::endl;
                    LDX(read(zeroPagedIndexedYAddress()));
                    pc += 2;
                    break;
                case 0xb8:
                    if (debug) std::cout << "CLV" << std::endl;
                    CLV();
                    pc += 1;
                    break;
                case 0xb9:
                    if (debug) std::cout << "LDA abs, Y" << std::endl;
                    LDA(read(absoluteIndexedY()));
                    pc += 3;
                    break;
                case 0xba:
                    if (debug) std::cout << "TSX" << std::endl;
                    TSX();
                    pc += 1;
                    break;
                case 0xbc:
                    if (debug) std::cout << "LDY abs, X" << std::endl;
                    LDY(read(absoluteIndexedX()));
                    pc += 3;
                    break;
                case 0xbd:  
                    if (debug) std::cout << "abs, X" << std::endl;
                    LDA(read(absoluteIndexedX()));
                    pc += 3;
                    break;
                case 0xbe:
                    if (debug) std::cout << "LDX, abs Y" << std::endl;
                    LDX(read(absoluteIndexedY()));
                    pc += 3;
                    break;

                case 0xc0:
                    if (debug) std::cout << "CPU #" << std::endl;
                    CPY(immediateValue());
                    pc += 2;
                    break;
                case 0xc1:
                    if (debug) std::cout << "CMP X, ind" << std::endl;
                    CMP(read(indexedIndirectAddress()));
                    pc += 2;
                    break; 
                case 0xc4:
                    if (debug) std::cout << "CPY zpg" << std::endl;
                    CPY(read(zeroPagedAddress()));
                    pc += 2;
                    break;
                case 0xc5:
                    if (debug) std::cout << "CMP zpg" << std::endl;
                    CMP(read(zeroPagedAddress()));
                    pc += 2;
                    break;
                case 0xc6:
                    if (debug) std::cout << "DEC zpg" << std::endl;
                    DEC(zeroPagedAddress());
                    pc += 2;
                    break;
                case 0xc8:  
                    if (debug) std::cout << "INY" << std::endl;
                    INY();
                    pc += 1;
                    break;
                case 0xc9:
                    if (debug) std::cout << "CMP #" << std::endl;
                    CMP(immediateValue());
                    pc += 2;
                    break;
                case 0xca:
                    if (debug) std::cout << "DEX" << std::endl;
                    DEX();
                    pc += 1;
                    break;
                case 0xcc:
                    if (debug) std::cout << "CPY abs" << std::endl;
                    CPY(read(absoluteAddress()));
                    pc += 3;
                    break;
                case 0xcd:  
                    if (debug) std::cout << "CMP abs" << std::endl;
                    CMP(read(absoluteAddress()));
                    pc += 3;
                    break;
                case 0xce:
                    if (debug) std::cout << "DEC abs" << std::endl;
                    DEC(absoluteAddress());
                    pc += 3;
                    break;
                
                case 0xd0:
                    if (debug) std::cout << "BNE rel" << std::endl;
                    BNE(relativeValue());
                    pc += 2;
                    break;
                case 0xd1:
                    if (debug) std::cout << "CMP ind, Y" << std::endl;
                    CMP(read(indirectIndexedAddress()));
                    pc += 2;
                    break;
                case 0xd5:
                    if (debug) std::cout << "CMP zpg, X" << std::endl;
                    CMP(read(zeroPagedIndexedXAddress()));
                    pc += 2;
                    break;
                case 0xd6:
                    if (debug) std::cout << "DEC zpg, X" << std::endl;
                    DEC(zeroPagedIndexedXAddress());
                    pc += 2;
                    break;
                case 0xd8:
                    if (debug) std::cout << "CLD" << std::endl;
                    CLD();
                    pc += 1;
                    break;
                case 0xd9:  
                    if (debug) std::cout << "CMP abs, Y" << std::endl;
                    CMP(read(absoluteIndexedY()));
                    pc += 3;
                    break;
                case 0xdd:
                    if (debug) std::cout << "CMP abs, X" << std::endl;
                    CMP(read(absoluteIndexedX()));
                    pc += 3;
                    break;
                case 0xde:
                    if (debug) std::cout << "DEC abs, X" << std::endl;
                    DEC(absoluteIndexedX());
                    pc += 3;
                    break;

                case 0xe0:
                    if (debug) std::cout << "CPX #" << std::endl;
                    CPX(immediateValue());
                    pc += 2;
                    break;
                case 0xe1:
                    if (debug) std::cout << "SBC X, ind" << std::endl;
                    SBC(read(indexedIndirectAddress()));
                    pc += 2;
                    break;
                case 0xe4:
                    if (debug) std::cout << "CPX zpg" << std::endl;
                    CPX(read(zeroPagedAddress()));
                    pc += 2;
                    break;
                case 0xe5:
                    if (debug) std::cout << "SBC zpg" << std::endl;
                    SBC(read(zeroPagedAddress()));
                    pc += 2;
                    break;
                case 0xe6:
                    if (debug) std::cout << "INC zpg" << std::endl;
                    INC(zeroPagedAddress());
                    pc += 2;
                    break;
                case 0xe8:
                    if (debug) std::cout << "INX" << std::endl;
                    INX();
                    pc += 1;
                    break;
                case 0xe9:
                    if (debug) std::cout << "SBC #" << std::endl;
                    SBC(immediateValue());
                    pc += 2;
                    break;
                case 0xea:
                    if (debug) std::cout << "NOP" << std::endl;
                    NOP();
                    pc += 1;
                    break;
                case 0xec:
                    if (debug) std::cout << "CPX abs" << std::endl;
                    CPX(read(absoluteAddress()));
                    pc += 3;
                    break;
                case 0xed:
                    if (debug) std::cout << "SBC abs" << std::endl;
                    SBC(read(absoluteAddress()));
                    pc += 3;
                    break;
                case 0xee:
                    if (debug) std::cout << "INC abs" << std::endl;
                    INC(absoluteAddress());
                    pc += 3;
                    break;

                case 0xf0:  
                    if (debug) std::cout << "BEQ rel" << std::endl;
                    BEQ(relativeValue());
                    pc += 2;
                    break;
                case 0xf1:  
                    if (debug) std::cout << "SBC ind, Y" << std::endl;
                    SBC(read(indirectIndexedAddress()));
                    pc += 2;
                    break;
                case 0xf5:
                    if (debug) std::cout << "EOR zpg, X" << std::endl;
                    SBC(read(zeroPagedIndexedXAddress()));
                    pc += 2;
                    break;
                case 0xf6:
                    if (debug) std::cout << "INC zpg, X" << std::endl;
                    INC(zeroPagedIndexedXAddress());
                    pc += 2;
                    break;
                case 0xf8:  
                    if (debug) std::cout << "SED" << std::endl;
                    SED();
                    pc += 1;
                    break;
                case 0xf9:
                    if (debug) std::cout << "SBC abs, Y" << std::endl;
                    SBC(read(absoluteIndexedY()));
                    pc += 3;
                    break;
                case 0xfd:
                    if (debug) std::cout << "SBC abs, X" << std::endl;
                    SBC(read(absoluteIndexedX()));
                    pc += 3;
                    break;
                case 0xfe:
                    if (debug) std::cout << "INC abs, X" << std::endl;
                    INC(absoluteIndexedX());
                    pc += 3;
                    break;

                default:
                    std::cout << "Unknown opcode! (" << (uint16_t)instr_reg << ")" << std::endl;
                    return false;
                    
            }
            return true;
        }
        
        // instructions

        void NOP() {}

        void BRK() {
            pc += 1;

            pushPC();
            pushStack(psr);

            pc = ((uint16_t)read(IRQ-1) << 8) | read(IRQ);

            setFlag(BREAK_FLAG);
            setFlag(INTERRUPT_FLAG);
        }

        void ORA(uint8_t operand) {
            accumulator |= operand;

            // Negative flag
            if (accumulator & 0x80 > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            // Zero flag    
            if (accumulator == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void ASL(uint16_t operand) {
            if ((read(operand) & 0x80) > 0) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);

            write(operand, read(operand) << 1);

            if (read(operand) == 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if ((read(operand) & 0x80) > 0) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);
        }

        void ASL() {
            if ((accumulator & 0x80) > 0) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);

            accumulator <<= 1;

            if ((accumulator & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (accumulator == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void PHP() {
            pushStack(psr);   
        }

        void BPL(uint8_t operand) {
            if (!checkFlag(NEGATIVE_FLAG)) pc += (int8_t)operand;
            else pc += 2;
        }

        void CLC() {
            unsetFlag(CARRY_FLAG);
        }

        void JSR(uint16_t operand) {
            pc += 3;

            pushPC();

            pc = operand;
        }

        void AND(uint8_t operand) { 
            accumulator &= operand;

            if ((accumulator & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (accumulator == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void BIT(uint8_t operand) {
            uint8_t temp = accumulator & operand;
            
            if ((temp & 0x40) > 0) setFlag(OVERFLOW_FLAG);
            else unsetFlag(OVERFLOW_FLAG);

            if ((temp & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (temp == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void ROL(uint8_t operand) {
            if ((read(operand) & 0x80) > 0) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);

            uint8_t temp = (read(operand) & 0x80) >> 7;

            write(operand, (read(operand) << 1) | temp);

            if ((read(operand) & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (read(operand) == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void ROL() {
            if ((accumulator & 0x80) > 0) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);

            uint8_t temp = (0x80 & accumulator) >> 7;

            accumulator = (accumulator << 1) | temp;

            if ((accumulator & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (accumulator == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void PLP() {
            psr = pullStack();
        }
        
        void BMI(uint8_t operand) {
            if (checkFlag(NEGATIVE_FLAG)) pc += (int8_t)operand;
            else pc += 2;
        }

        void SEC() {
            setFlag(CARRY_FLAG);
        }

        void RTI() {
            psr = pullStack();
            pc = pullPC();
        }

        void EOR(uint8_t operand) {
            accumulator ^= operand;

            if ((accumulator & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (accumulator == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void LSR(uint8_t operand) {
            if ((read(operand) & 0x80) > 0) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);
            
            write(operand, read(operand) >> 1);

            if (read(operand) == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);

            unsetFlag(NEGATIVE_FLAG);
        }

        void LSR() {
            if ((accumulator & 0x80) > 0) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);

            accumulator >>= 1;

            if (accumulator == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);

            unsetFlag(NEGATIVE_FLAG);
        }

        void PHA() {
            pushStack(accumulator);
        }

        void JMP(uint16_t operand) {
            pc = operand;
        }

        void BVC(uint8_t operand) {
            if (!checkFlag(NEGATIVE_FLAG)) pc += (int8_t)operand;
            else pc += 2;
        }

        void CLI() {
            unsetFlag(INTERRUPT_FLAG);
        }

        void RTS() {
            pc = pullPC();
        }

        void ADC(uint8_t operand) {
            uint8_t temp = accumulator;

            accumulator += operand;

            if (checkFlag(DECIMAL_FLAG)) {
                uint16_t bcdTemp = accumulator;

                if ((bcdTemp & 0x0f) > 0x09) 
                    bcdTemp += 0x06;

                if ((bcdTemp & 0xf0) > 0x90) 
                    bcdTemp += 0x60;

                if (bcdTemp > 0x99) setFlag(CARRY_FLAG);
                else unsetFlag(CARRY_FLAG);

                accumulator = bcdTemp;
            } else {
                if ((uint16_t)temp + operand > 255) setFlag(CARRY_FLAG);
                else unsetFlag(CARRY_FLAG);
            }
            
            bool signTemp = (temp & 0x80) != 0;
            bool signOperand = (operand & 0x80) != 0;
            bool signAccumulator = (accumulator & 0x80) != 0;

            if ((signTemp == signOperand) && (signAccumulator != signTemp)) setFlag(OVERFLOW_FLAG);
            else unsetFlag(OVERFLOW_FLAG);

            if ((accumulator & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (accumulator == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void ROR(uint8_t operand) {
            if ((read(operand) & 0x01) > 0) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);

            uint8_t temp = read(operand) & 0x01;

            write(operand, (read(operand) >> 1) | temp);

            if ((read(operand) & 0x08) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (read(operand) == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void ROR() {
            if ((accumulator & 0x01) > 0) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);

            uint8_t temp = accumulator & 0x01;

            accumulator = (accumulator >> 1) | temp;

            if ((accumulator & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (accumulator == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void PLA() {
            accumulator = pullStack();

            if ((accumulator & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (accumulator == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void BVS(uint8_t operand) {
            if (checkFlag(OVERFLOW_FLAG)) pc += (int8_t)operand;
            else pc += 2;
        }

        void STA(uint16_t operand) {
            write(operand, accumulator);
        }

        void STY(uint16_t operand) {
            write(operand, y);
        }

        void STX(uint16_t operand) {
            write(operand, x);
        }

        void DEY() {
            y--;
            
            if ((y & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (y == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void TXA() {
            accumulator = x;

            if ((accumulator & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (accumulator == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void BCC(uint8_t operand) {
            if (!checkFlag(CARRY_FLAG)) pc += (int8_t)operand;
            else pc += 2;
        }

        void TYA() {
            accumulator = y;

            if ((accumulator & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (accumulator == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void TXS() {
            sp = x;
        }

        void LDY(uint8_t operand) {
            y = operand;

            if ((y & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (y == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void LDA(uint8_t operand) {
            accumulator = operand;

            if ((accumulator & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (accumulator == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);           
        }

        void LDX(uint8_t operand) {
            x = operand;

            if ((x & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (x == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void TAY() {
            y = accumulator;

            if ((y & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (y == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void TAX() {
            x = accumulator;

            if ((x & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (x == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void BCS(uint8_t operand) {
            if (checkFlag(CARRY_FLAG)) pc += (int8_t)operand;
            else pc += 2;
        }

        void CLV() {
            unsetFlag(OVERFLOW_FLAG);
        }

        void TSX() {
            sp = x;
        }

        void CPY(uint8_t operand) {
            if (y >= operand) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);

            uint8_t temp = y - operand;

            if ((temp & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (temp == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void CMP(uint8_t operand) {
            if (accumulator > operand) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);

            uint8_t temp = accumulator - operand;

            if ((temp & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (temp == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void DEC(uint16_t operand) {
            write(operand, read(operand)-1);

            if ((read(operand) & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (read(operand) == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void INY() {
            y++;

            if ((y & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (y == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void DEX() {
            x--;

            if ((x & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (x == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void BNE(uint8_t operand) {
            if (!checkFlag(ZERO_FLAG)) pc += (int8_t)operand;
            else pc += 2;
        }

        void CLD() {
            unsetFlag(DECIMAL_FLAG);
        }

        void CPX(uint8_t operand) {
            if (x > operand) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);

            uint8_t temp = x - operand;

            if ((temp & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (temp == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void SBC(uint8_t operand) {
            uint8_t oldA = accumulator;
            uint16_t value = operand ^ 0xFF;
            uint16_t temp = (uint16_t)oldA + value + (checkFlag(CARRY_FLAG) ? 1 : 0);

            bool overflow = ((oldA ^ temp) & (operand ^ temp) & 0x80) != 0;
            if (overflow) setFlag(OVERFLOW_FLAG); else unsetFlag(OVERFLOW_FLAG);

            if (checkFlag(DECIMAL_FLAG)) {
                uint16_t correction = 0;

                if (((oldA & 0x0F) + (value & 0x0F) + (checkFlag(CARRY_FLAG) ? 1 : 0)) > 0x09)
                    correction += 0x06;
                if ((temp > 0x99))
                    correction += 0x60;

                temp += correction;
            }

            if (temp & 0x100) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);

            accumulator = (uint8_t)(temp & 0xFF);

            if ((accumulator & 0x80) > 0) setFlag(NEGATIVE_FLAG); 
            else unsetFlag(NEGATIVE_FLAG); 
            
            if (accumulator == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void INC(uint16_t operand) {
            write(operand, read(operand)+1);

            if ((read(operand) & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (read(operand) == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void INX() {
            x++;

            if ((x & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (x == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void BEQ(uint8_t operand) {
            if (checkFlag(ZERO_FLAG)) pc += (int8_t)operand;
            else pc += 2;
        }

        void SED() {
            setFlag(DECIMAL_FLAG);
        }
};

int main() {
    FILE* f = fopen("roms/test.bin", "rb");
    fseek(f, 0, SEEK_END);
    const int size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* buffer = new uint8_t[size];
    fread(buffer, size, 1, f);
    for (int i = 0; i < size; i++) {
        write(i, buffer[i]);
    }
    fclose(f);
    delete[] buffer;

    // peripherals.push_back(new PeripheralA);

    CPU a(true);

    a.run();

    // for (auto *peripheral: peripherals) {
    //     delete peripheral;
    // }

    return 0;
}