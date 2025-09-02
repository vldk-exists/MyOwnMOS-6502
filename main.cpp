#include <iostream>
#include <cstdint>
#include <fstream>

/* 
Sources:
    - https://ru.wikipedia.org/wiki/MOS_Technology_6502
    - https://www.masswerk.at/6502/6502_instruction_set.html
    - https://www.masswerk.at/6502/assembler.html
*/


uint8_t memory[0xffff];

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

    public:
        uint8_t accumulator = 0;
        uint8_t x = 0, y = 0;
        uint8_t sp = 0; // 0x01 + sp;
        uint8_t psr = 0;
        
        uint8_t instr_reg = 0;

        uint16_t pc = 0;

        void pushStack(uint8_t data) {
            memory[((uint16_t)0x01 << 8) | sp] = data;
            sp--;
        }

        uint8_t pullStack() {
            sp++;
            return memory[((uint16_t)0x01 << 8) | sp];
        }

        void pushPC() {
            uint8_t high = pc >> 8;
            uint8_t low = pc & 0x0f;

            pushStack(high);
            pushStack(low);
        }

        uint16_t pullPC() {
            uint8_t low = pullStack();
            uint8_t high = pullStack();

            return ((uint16_t)high << 8) | low;
        }

        void reset() {
            pc = ((uint16_t)memory[RESET] << 8) | memory[RESET-1];
        }

        uint16_t absoluteAddress() {
            return ((uint16_t)memory[pc+1] << 8) | memory[pc+2];
        }

        uint8_t immediateValue() {
            return memory[pc + 1];
        }

        uint8_t relativeValue() {
            return memory[pc + 1];
        }

        uint16_t absoluteIndexedY() {
            return absoluteAddress()+y;
        }

        uint16_t absoluteIndexedX() {
            return absoluteAddress()+x;
        }
        
        uint16_t indirectAbsoluteAddress() {
            uint8_t low = memory[absoluteAddress()];
            uint16_t high = memory[absoluteAddress()+1] << 8;
            return high | low;
        }

        uint16_t indirectIndexedAddress() {
            uint8_t low = memory[pc+1];
            uint8_t high = memory[pc+2];
            uint16_t address = ((uint16_t)high << 8) | low;
            return address+y;
        }

        uint16_t indexedIndirectAddress() {
            uint8_t zeroPage = memory[pc+1] + x;
            uint8_t low = memory[zeroPage];
            uint8_t high = memory[zeroPage+1];

            return ((uint16_t)high << 8) | low;
        }

        uint16_t zeroPagedIndexedXAddress() {
            return memory[pc+1]+x;
        }

        uint16_t zeroPagedIndexedYAddress() {
            return memory[pc+1]+y;
        }

        uint8_t zeroPagedAddress() {
            return memory[pc+1];
        }
        
        void executeIRQ() {
            pushPC();
            pushStack(psr);

            pc = ((uint16_t)memory[IRQ-1] << 8) | memory[IRQ];

            setFlag(INTERRUPT_FLAG);

            isIRQ = false;
        }

        void executeNMI() {
            pushPC();
            pushStack(psr);

            pc = ((uint16_t)memory[NMI-1] << 8) | memory[NMI];

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
            std::cout << pc << std::endl;
            while (true) {
                if (isIRQ && !checkFlag(INTERRUPT_FLAG)) {
                    executeIRQ();
                } else if (isNMI) {
                    executeNMI();
                } 
                
                instr_reg = memory[pc];

                if (!decode()) return;
            }
        }

        bool decode() {
            switch (instr_reg) {
                case 0x00:
                    std::cout << "BRK" << std::endl;
                    BRK();
                    break;
                case 0x01:
                    std::cout << "ORA X, ind" << std::endl;
                    ORA(memory[indirectIndexedAddress()]);
                    pc += 2;
                    break;
                case 0x05:
                    std::cout << "ORA zpg" << std::endl;
                    ORA(memory[zeroPagedAddress()]);
                    pc += 2;
                    break;
                case 0x06:
                    std::cout << "ASL zpg" << std::endl;
                    ASL(memory[zeroPagedAddress()]);
                    pc += 2;
                    break;
                case 0x08:
                    std::cout << "PHP" << std::endl;
                    PHP();
                    pc += 1;
                    break;
                case 0x09:
                    std::cout << "ORA #" << std::endl;
                    ORA(immediateValue());
                    pc += 2;
                    break;
                case 0x0a:
                    std::cout << "ASL A" << std::endl;
                    ASL();
                    pc += 1;
                    break;
                case 0x0d:
                    std::cout << "ORA abs" << std::endl;
                    ORA(memory[absoluteAddress()]);
                    pc += 3;
                    break;
                case 0x0e:
                    std::cout << "ASL abs" << std::endl;
                    ASL(absoluteAddress());
                    pc += 3;
                    break;

                case 0x10:
                    std::cout << "BPL rel" << std::endl;
                    BPL(relativeValue());
                    break;
                case 0x11:
                    std::cout << "ORA ind, Y" << std::endl;
                    ORA(memory[indirectIndexedAddress()]);
                    pc += 2;
                    break;
                case 0x15:
                    std::cout << "ORA zpg, X" << std::endl;
                    ORA(memory[zeroPagedIndexedXAddress()]);
                    pc += 2;
                    break;
                case 0x16:
                    std::cout << "ASL zpg, X" << std::endl;
                    ASL(zeroPagedIndexedXAddress());
                    pc += 2;
                    break;
                case 0x18:
                    std::cout << "CLC" << std::endl;
                    CLC();
                    pc += 1;
                    break;
                case 0x19:
                    std::cout << "ORA abs, Y" << std::endl;
                    ORA(memory[absoluteIndexedY()]);
                    pc += 3;
                    break;
                case 0x1d:
                    std::cout << "ORA abs, X" << std::endl;
                    ORA(memory[absoluteIndexedX()]);
                    pc += 3;
                    break;
                case 0x1e:
                    std::cout << "ASL abs, X" << std::endl;
                    ASL(absoluteIndexedX());
                    pc += 3;
                    break;

                case 0x20:
                    std::cout << "JSR abs" << std::endl;
                    JSR(absoluteAddress());
                    break;
                case 0x21:
                    std::cout << "AND X, ind" << std::endl;
                    AND(memory[indirectIndexedAddress()]);
                    pc += 2;
                    break;
                case 0x24:
                    std::cout << "BIT zpg" << std::endl;
                    BIT(memory[zeroPagedAddress()]);
                    pc += 2;
                    break;
                case 0x25:
                    std::cout << "AND zpg" << std::endl;
                    AND(memory[zeroPagedIndexedXAddress()]);
                    pc += 2;
                    break;
                case 0x26:
                    std::cout << "ROL zpg" << std::endl;
                    ROL(zeroPagedAddress());
                    pc += 2;
                    break;
                case 0x28:
                    std::cout << "PLP" << std::endl;
                    PLP();
                    pc += 1;
                    break;
                case 0x29:
                    std::cout << "AND #" << std::endl;
                    AND(immediateValue());
                    pc += 1;
                    break;
                case 0x2a:
                    std::cout << "ROL A" << std::endl;
                    ROL();
                    pc += 1;
                    break;
                case 0x2c:
                    std::cout << "BIT abs" << std::endl;
                    BIT(memory[absoluteAddress()]);
                    pc += 3;
                    break;
                case 0x2d:
                    std::cout << "AND abs" << std::endl;
                    AND(memory[absoluteAddress()]);
                    pc += 3;
                    break;
                case 0x2e:
                    std::cout << "ROL abs" << std::endl;
                    ROL(absoluteAddress());
                    pc += 3;
                    break;

                case 0x30:
                    std::cout << "BMI rel" << std::endl;
                    BMI(relativeValue());
                    break;
                case 0x31:
                    std::cout << "AND ind, Y" << std::endl;
                    AND(memory[indirectIndexedAddress()]);
                    pc += 2;
                    break;
                case 0x35:
                    std::cout << "AND zpg, X" << std::endl;
                    AND(memory[zeroPagedIndexedXAddress()]);
                    pc += 2;
                    break;
                case 0x36:
                    std::cout << "ROL zpg, X" << std::endl;
                    ROL(zeroPagedAddress());
                    pc += 2;
                    break;
                case 0x38:
                    std::cout << "SEC" << std::endl;
                    SEC();
                    pc += 1;
                    break;
                case 0x39:
                    std::cout << "AND abs, Y" << std::endl;
                    AND(memory[absoluteIndexedY()]);
                    pc += 3;
                    break;
                case 0x3d:
                    std::cout << "AND abs, X" << std::endl;
                    AND(memory[absoluteIndexedX()]);
                    pc += 3;
                    break;
                case 0x3e:
                    std::cout << "ROL abs, X" << std::endl;
                    ROL(memory[absoluteIndexedX()]);
                    pc += 3;
                    break;

                case 0x40:
                    std::cout << "RTI" << std::endl;
                    RTI();
                    break;
                case 0x41:
                    std::cout << "EOR X, ind" << std::endl;
                    EOR(memory[indexedIndirectAddress()]);
                    pc += 2;
                    break;
                case 0x45:
                    std::cout << "EOR zpg" << std::endl;
                    EOR(memory[zeroPagedAddress()]);
                    pc += 2;
                    break;
                case 0x46:
                    std::cout << "LSR zpg" << std::endl;
                    LSR(zeroPagedAddress());
                    pc += 2;
                    break;
                case 0x48:
                    std::cout << "PHA" << std::endl;
                    PHA();
                    pc += 1;
                    break;
                case 0x49:
                    std::cout << "EOR #" << std::endl;
                    EOR(immediateValue());
                    pc += 2;
                    break;
                case 0x4a:
                    std::cout << "LSR A" << std::endl;
                    LSR();
                    pc += 1;
                    break;
                case 0x4c:
                    std::cout << "JMP abs" << std::endl;
                    JMP(absoluteAddress());
                    pc += 3;
                    break;
                case 0x4d:
                    std::cout << "EOR abs" << std::endl;
                    EOR(memory[absoluteAddress()]);
                    pc += 3;
                    break;
                case 0x4e:
                    std::cout << "LSR abs" << std::endl;
                    LSR(absoluteAddress());
                    pc += 3;
                    break;
                
                case 0x50:
                    std::cout << "BVC rel" << std::endl;
                    BVC(relativeValue());
                    break;
                case 0x51:
                    std::cout << "EOR ind, Y" << std::endl;
                    EOR(memory[indirectIndexedAddress()]);
                    pc += 2;
                    break;
                case 0x55:
                    std::cout << "EOR zpg, X" << std::endl;
                    EOR(memory[zeroPagedIndexedXAddress()]);
                    pc += 2;
                    break;
                case 0x56:
                    std::cout << "LSR zpg, X" << std::endl;
                    LSR(zeroPagedIndexedXAddress());
                    pc += 2;
                    break;
                case 0x58:
                    std::cout << "CLI" << std::endl;
                    CLI();
                    pc += 1;
                    break;
                case 0x59:
                    std::cout << "EOR abs, Y" << std::endl;
                    EOR(memory[absoluteIndexedY()]);
                    pc += 3;
                    break;
                case 0x5d:
                    std::cout << "EOR abs, X" << std::endl;
                    EOR(memory[absoluteIndexedX()]);
                    pc += 3;
                    break;
                case 0x5e:
                    std::cout << "LSR abs, X" << std::endl;
                    LSR(absoluteIndexedX());
                    pc += 3;
                    break;
                
                case 0x60:
                    std::cout << "RTS" << std::endl;
                    RTS();
                    break;
                case 0x61:
                    std::cout << "ADC X, ind" << std::endl;
                    ADC(memory[indexedIndirectAddress()]);
                    pc += 2;
                    break;
                case 0x65:
                    std::cout << "ADC zpg" << std::endl;
                    ADC(memory[zeroPagedAddress()]);
                    pc += 2;
                    break;
                case 0x66:
                    std::cout << "ROR zpg" << std::endl;
                    ROR(zeroPagedAddress());
                    pc += 2;
                    break;
                case 0x68:
                    std::cout << "PLA" << std::endl;
                    PLA();
                    pc += 2;
                    break;
                case 0x69:
                    std::cout << "ADC #" << std::endl;
                    ADC(immediateValue());
                    pc += 2;
                    break;
                case 0x6a:
                    std::cout << "ROR A" << std::endl;
                    ROR();
                    pc += 2;
                    break;
                case 0x6c:
                    std::cout << "JMP ind" << std::endl;
                    JMP(indirectAbsoluteAddress());
                    break;
                case 0x6d:
                    std::cout << "ADC abs" << std::endl;
                    ADC(memory[absoluteAddress()]);
                    pc += 3;
                    break;
                case 0x6e:
                    std::cout << "ROR abs" << std::endl;
                    ROR(absoluteAddress());
                    pc += 3;
                    break;
                
                case 0x70:
                    std::cout << "BVS rel" << std::endl;
                    BVS(relativeValue());
                    pc += 2;
                    break;
                case 0x71:
                    std::cout << "ADC ind, Y" << std::endl;
                    ADC(memory[indirectIndexedAddress()]);
                    pc += 2;
                    break;                
                case 0x75:
                    std::cout << "ADC zpg, X" << std::endl;
                    ADC(memory[zeroPagedIndexedXAddress()]);
                    pc += 2;
                    break;
                case 0x76:
                    std::cout << "ROR zpg, X" << std::endl;
                    ROR(zeroPagedIndexedXAddress());
                    pc += 2;
                    break;
                case 0x7d:
                    std::cout << "ADC abs, X" << std::endl;
                    ADC(memory[absoluteIndexedX()]);
                    pc += 3;
                    break;
                case 0x7e:
                    std::cout << "ROR abs, X" << std::endl;
                    ROR(memory[absoluteIndexedX()]);
                    pc += 3;
                    break;

                case 0x81:
                    std::cout << "STA X, ind" << std::endl;
                    STA(indexedIndirectAddress());
                    pc += 2;
                    break;
                case 0x84:
                    std::cout << "STY zpg" << std::endl;
                    STY(zeroPagedAddress());
                    pc += 2;
                    break;
                case 0x85:
                    std::cout << "STA zpg" << std::endl;
                    STA(zeroPagedAddress());
                    pc += 2;
                    break;
                case 0x86:
                    std::cout << "STX, zpg" << std::endl;
                    STX(zeroPagedAddress());
                    pc += 2;
                    break;
                case 0x88:
                    std::cout << "DEY" << std::endl;
                    DEY();
                    pc += 1;
                    break;
                case 0x8a:
                    std::cout << "TXA" << std::endl;
                    TXA();
                    pc += 1;
                    break;
                case 0x8c:
                    std::cout << "STY abs" << std::endl;
                    STY(absoluteAddress());
                    pc += 3;
                    break;
                case 0x8d:
                    std::cout << "STA abs" << std::endl;
                    STA(absoluteAddress());
                    pc += 3;
                    break;
                case 0x8e:
                    std::cout << "STX abs" << std::endl;
                    STX(absoluteAddress());
                    pc += 3;
                    break;

                case 0x90:
                    std::cout << "BCC rel" << std::endl;
                    BCC(relativeValue());
                    pc += 2;
                    break;
                case 0x91:
                    std::cout << "STA X, ind" << std::endl;
                    STA(indexedIndirectAddress());
                    pc += 2;
                    break;
                case 0x94:
                    std::cout << "STY zpg, X" << std::endl;
                    STY(zeroPagedIndexedXAddress());
                    pc += 2;
                    break;
                case 0x95:
                    std::cout << "STA zpg, X" << std::endl;
                    STA(zeroPagedIndexedXAddress());
                    pc += 2;
                    break;
                case 0x96:
                    std::cout << "STX zpg, Y" << std::endl;
                    STX(zeroPagedIndexedYAddress());
                    pc += 2;
                    break;
                case 0x98:
                    std::cout << "TYA" << std::endl;
                    TYA();
                    pc += 1;
                    break;
                case 0x99:
                    std::cout << "STA abs, Y" << std::endl;
                    STA(absoluteIndexedY());
                    pc += 3;
                    break;
                case 0x9a:
                    std::cout << "TXS" << std::endl;
                    TXS();
                    pc += 1;
                    break;
                case 0x9d:
                    std::cout << "STA abs, X" << std::endl;
                    STA(absoluteIndexedX());
                    pc += 3;
                    break;
                
                case 0xa0:
                    std::cout << "LDY #" << std::endl;
                    LDY(immediateValue());
                    pc += 2;
                    break;
                case 0xa1:
                    std::cout << "LDA X, ind" << std::endl;
                    LDA(memory[indexedIndirectAddress()]);
                    pc += 2;
                    break;
                case 0xa2:
                    std::cout << "LDX #" << std::endl;
                    LDX(immediateValue());
                    pc += 2;
                    break;
                case 0xa4:
                    std::cout << "LDY zpg" << std::endl;
                    LDY(memory[zeroPagedAddress()]);
                    pc += 2;
                    break;
                case 0xa5:
                    std::cout << "LDA zpg" << std::endl;
                    LDA(memory[zeroPagedAddress()]);
                    pc += 2;
                    break;
                case 0xa6:
                    std::cout << "LDX zpg" << std::endl;
                    LDX(memory[zeroPagedAddress()]);
                    pc += 2;
                    break;
                case 0xa8:
                    std::cout << "TAY" << std::endl;
                    TAY();
                    pc += 1;
                    break;
                case 0xa9:
                    std::cout << "LDA #" << std::endl;
                    LDA(immediateValue());
                    pc += 2;
                    break;
                case 0xaa:
                    std::cout << "TAX" << std::endl;
                    TAX();
                    pc += 1;
                    break;
                case 0xac:
                    std::cout << "LDY abs" << std::endl;
                    LDY(memory[absoluteAddress()]);
                    pc += 3;
                    break;
                case 0xad:
                    std::cout << "LDA abs" << std::endl;
                    LDA(memory[absoluteAddress()]);
                    pc += 3;
                    break;
                case 0xae:
                    std::cout << "LDX abs" << std::endl;
                    LDX(memory[absoluteAddress()]);
                    pc += 3;
                    break;

                case 0xb0:
                    std::cout << "BCS rel" << std::endl;
                    BCS(relativeValue());
                    pc += 2;
                    break;
                case 0xb1:
                    std::cout << "LDA ind, Y" << std::endl;
                    LDA(memory[indirectIndexedAddress()]);
                    pc += 2;
                    break;
                case 0xb4:
                    std::cout << "LDY zpg, X" << std::endl;
                    LDY(memory[zeroPagedIndexedXAddress()]);
                    pc += 2;
                    break;
                case 0xb5:
                    std::cout << "LDA zpg, X" << std::endl;
                    LDA(memory[zeroPagedIndexedXAddress()]);
                    pc += 2;
                    break;
                case 0xb6:
                    std::cout << "LDX zpg, X" << std::endl;
                    LDX(memory[zeroPagedIndexedYAddress()]);
                    pc += 2;
                    break;
                case 0xb8:
                    std::cout << "CLV" << std::endl;
                    CLV();
                    pc += 1;
                    break;
                case 0xb9:
                    std::cout << "LDA abs, Y" << std::endl;
                    LDA(memory[absoluteIndexedY()]);
                    pc += 3;
                    break;
                case 0xba:
                    std::cout << "TSX" << std::endl;
                    TSX();
                    pc += 1;
                    break;
                case 0xbc:
                    std::cout << "LDY abs, X" << std::endl;
                    LDY(memory[absoluteIndexedX()]);
                    pc += 3;
                    break;
                case 0xbd:  
                    std::cout << "abs, X" << std::endl;
                    LDA(memory[absoluteIndexedX()]);
                    pc += 3;
                    break;
                case 0xbe:
                    std::cout << "LDX, abs Y" << std::endl;
                    LDX(memory[absoluteIndexedY()]);
                    pc += 3;
                    break;

                case 0xc0:
                    std::cout << "CPU #" << std::endl;
                    CPY(immediateValue());
                    pc += 2;
                    break;
                case 0xc1:
                    std::cout << "CMP X, ind" << std::endl;
                    CMP(memory[indexedIndirectAddress()]);
                    pc += 2;
                    break; 
                case 0xc4:
                    std::cout << "CPY zpg" << std::endl;
                    CPY(memory[zeroPagedAddress()]);
                    pc += 2;
                    break;
                case 0xc5:
                    std::cout << "CMP zpg" << std::endl;
                    CMP(memory[zeroPagedAddress()]);
                    pc += 2;
                    break;
                case 0xc6:
                    std::cout << "DEC zpg" << std::endl;
                    DEC(zeroPagedAddress());
                    pc += 2;
                    break;
                case 0xc8:  
                    std::cout << "INY" << std::endl;
                    INY();
                    pc += 1;
                    break;
                case 0xc9:
                    std::cout << "CMP #" << std::endl;
                    CMP(immediateValue());
                    pc += 2;
                    break;
                case 0xca:
                    std::cout << "DEX" << std::endl;
                    DEX();
                    pc += 1;
                    break;
                case 0xcc:
                    std::cout << "CPY abs" << std::endl;
                    CPY(memory[absoluteAddress()]);
                    pc += 3;
                    break;
                case 0xcd:  
                    std::cout << "CMP abs" << std::endl;
                    CMP(memory[absoluteAddress()]);
                    pc += 3;
                    break;
                case 0xce:
                    std::cout << "DEC abs" << std::endl;
                    DEC(absoluteAddress());
                    pc += 3;
                    break;
                
                case 0xd0:
                    std::cout << "BNE rel" << std::endl;
                    BNE(relativeValue());
                    pc += 2;
                    break;
                case 0xd1:
                    std::cout << "CMP ind, Y" << std::endl;
                    CMP(memory[indirectIndexedAddress()]);
                    pc += 2;
                    break;
                case 0xd5:
                    std::cout << "CMP zpg, X" << std::endl;
                    CMP(memory[zeroPagedIndexedXAddress()]);
                    pc += 2;
                    break;
                case 0xd6:
                    std::cout << "DEC zpg, X" << std::endl;
                    DEC(zeroPagedIndexedXAddress());
                    pc += 2;
                    break;
                case 0xd8:
                    std::cout << "CLD" << std::endl;
                    CLD();
                    pc += 1;
                    break;
                case 0xd9:  
                    std::cout << "CMP abs, Y" << std::endl;
                    CMP(memory[absoluteIndexedY()]);
                    pc += 3;
                    break;
                case 0xdd:
                    std::cout << "CMP abs, X" << std::endl;
                    CMP(memory[absoluteIndexedX()]);
                    pc += 3;
                    break;
                case 0xde:
                    std::cout << "DEC abs, X" << std::endl;
                    DEC(absoluteIndexedX());
                    pc += 3;
                    break;

                case 0xe0:
                    std::cout << "CPX #" << std::endl;
                    CPX(immediateValue());
                    pc += 2;
                    break;
                case 0xe1:
                    std::cout << "SBC X, ind" << std::endl;
                    SBC(memory[indexedIndirectAddress()]);
                    pc += 2;
                    break;
                case 0xe4:
                    std::cout << "CPX zpg" << std::endl;
                    CPX(memory[zeroPagedAddress()]);
                    pc += 2;
                    break;
                case 0xe5:
                    std::cout << "SBC zpg" << std::endl;
                    SBC(memory[zeroPagedAddress()]);
                    pc += 2;
                    break;
                case 0xe6:
                    std::cout << "INC zpg" << std::endl;
                    INC(zeroPagedAddress());
                    pc += 2;
                    break;
                case 0xe8:
                    std::cout << "INX" << std::endl;
                    INX();
                    pc += 1;
                    break;
                case 0xe9:
                    std::cout << "SBC #" << std::endl;
                    SBC(immediateValue());
                    pc += 2;
                    break;
                case 0xea:
                    std::cout << "NOP" << std::endl;
                    NOP();
                    pc += 1;
                    break;
                case 0xec:
                    std::cout << "CPX abs" << std::endl;
                    CPX(memory[absoluteAddress()]);
                    pc += 3;
                    break;
                case 0xed:
                    std::cout << "SBC abs" << std::endl;
                    SBC(memory[absoluteAddress()]);
                    pc += 3;
                    break;
                case 0xee:
                    std::cout << "INC abs" << std::endl;
                    INC(absoluteAddress());
                    pc += 3;
                    break;

                case 0xf0:  
                    std::cout << "BEQ rel" << std::endl;
                    BEQ(relativeValue());
                    pc += 2;
                    break;
                case 0xf1:  
                    std::cout << "SBC ind, Y" << std::endl;
                    SBC(memory[indirectIndexedAddress()]);
                    pc += 2;
                    break;
                case 0xf5:
                    std::cout << "EOR zpg, X" << std::endl;
                    SBC(memory[zeroPagedIndexedXAddress()]);
                    pc += 2;
                    break;
                case 0xf6:
                    std::cout << "INC zpg, X" << std::endl;
                    INC(zeroPagedIndexedXAddress());
                    pc += 2;
                    break;
                case 0xf8:  
                    std::cout << "SED" << std::endl;
                    SED();
                    pc += 1;
                    break;
                case 0xf9:
                    std::cout << "SBC abs, Y" << std::endl;
                    SBC(memory[absoluteIndexedY()]);
                    pc += 3;
                    break;
                case 0xfd:
                    std::cout << "SBC abs, X" << std::endl;
                    SBC(memory[absoluteIndexedX()]);
                    pc += 3;
                    break;
                case 0xfe:
                    std::cout << "INC abs, X" << std::endl;
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

            pc = ((uint16_t)memory[IRQ-1] << 8) | memory[IRQ];

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

        void ASL(uint8_t operand) {
            if ((memory[operand] & 0x80) > 0) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);

            memory[operand] <<= 1;

            if (memory[operand] == 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if ((memory[operand] & 0x80) > 0) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);
        }

        void ASL(uint16_t operand) {
            if ((memory[operand] & 0x80) > 0) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);

            memory[operand] <<= 1;

            if (memory[operand] == 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if ((memory[operand] & 0x80) > 0) setFlag(CARRY_FLAG);
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
            if ((memory[operand] & 0x80) > 0) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);

            uint8_t temp = (memory[operand] & 0x80) >> 7;

            memory[operand] = (memory[operand] << 1) | temp;

            if ((memory[operand] & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (memory[operand] == 0) setFlag(ZERO_FLAG);
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
            if ((memory[operand] & 0x80) > 0) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);

            memory[operand] >>= 1;

            if (memory[operand] == 0) setFlag(ZERO_FLAG);
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
            if ((memory[operand] & 0x01) > 0) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);

            uint8_t temp = memory[operand] & 0x01;

            memory[operand] = (memory[operand] >> 1) | temp;

            if ((memory[operand] & 0x08) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (memory[operand] == 0) setFlag(ZERO_FLAG);
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
            memory[operand] = accumulator;
        }

        void STY(uint16_t operand) {
            memory[operand] = y;
        }

        void STX(uint16_t operand) {
            memory[operand] = x;
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
            memory[operand]--;

            if ((memory[operand] & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (memory[operand] == 0) setFlag(ZERO_FLAG);
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
            memory[operand]++;

            if ((memory[operand] & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (memory[operand] == 0) setFlag(ZERO_FLAG);
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
    FILE* f = fopen("roms/b.bin", "rb");
    fseek(f, 0, SEEK_END);
    const int size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* buffer = new uint8_t[size];
    fread(buffer, size, 1, f);
    for (int i = 0; i < size; i++) {
        memory[i] = buffer[i];
    }
    fclose(f);
    delete[] buffer;

    CPU a;

    a.run();

    return 0;
}