#include <iostream>
#include <cstdint>

// https://ru.wikipedia.org/wiki/Apple_II
// https://ru.wikipedia.org/wiki/MOS_Technology_6502
// https://www.masswerk.at/6502/6502_instruction_set.html
// https://github.com/gianlucag/mos6502

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

// it's little-endian processor, remember it!
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

        CPU() { 
            // memory[IRQ] = ...
            // memory[IRQ-1] = ...

            // memory[RESET] = 0x02; // high
            // memory[RESET-1] = 0x00; // low   

            // memory[NMI] = ...
            // memory[NMI-1]
        }

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
            pc = ((uint16_t)memory[RESET-1] << 8) | memory[RESET];
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
            while (true) {
                if (isIRQ && !checkFlag(INTERRUPT_FLAG)) {
                    executeIRQ();
                } else if (isNMI) {
                    executeNMI();
                } 
                
                instr_reg = memory[pc];

                decode();
            }
        }

        void decode() {
            switch (instr_reg) {
                case 0x00:
                    BRK();
                    break;
                case 0x01:
                    pc += 2;
                    ORA(memory[indirectIndexedAddress()]);
                    break;
                case 0x05:
                    pc += 2;
                    ORA(memory[zeroPagedAddress()]);
                    break;
                case 0x06:
                    pc += 2;
                    ASL(memory[zeroPagedAddress()]);
                    break;
                case 0x08:
                    pc += 1;
                    PHP();
                    break;
                case 0x09:
                    pc += 2;
                    ORA(immediateValue());
                    break;
                case 0x0a:
                    pc += 1;
                    ASL();
                    break;
                case 0x0d:
                    pc += 3;
                    ORA(memory[absoluteAddress()]);
                    break;
                case 0x0e:
                    pc += 3;
                    ASL(absoluteAddress());
                    break;

                case 0x10:
                    BPL(relativeValue());
                    break;
                case 0x11:
                    pc += 2;
                    ORA(memory[indirectIndexedAddress()]);
                    break;
                case 0x15:
                    pc += 2;
                    ORA(memory[zeroPagedIndexedXAddress()]);
                    break;
                case 0x16:
                    pc += 2;
                    ASL(zeroPagedIndexedXAddress());
                    break;
                case 0x18:
                    pc += 1;
                    CLC();
                    break;
                case 0x19:
                    pc += 3;
                    ORA(memory[absoluteIndexedY()]);
                    break;
                case 0x1d:
                    pc += 3;
                    ORA(memory[absoluteIndexedX()]);
                    break;
                case 0x1e:
                    pc += 3;
                    ASL(absoluteIndexedX());
                    break;

                case 0x20:
                    JSR(absoluteAddress());
                    break;
                case 0x21:
                    pc += 2;
                    AND(memory[indirectIndexedAddress()]);
                    break;
                case 0x24:
                    pc += 2;
                    BIT(memory[zeroPagedAddress()]);
                    break;
                case 0x25:
                    pc += 2;
                    AND(memory[zeroPagedIndexedXAddress()]);
                    break;
                case 0x26:
                    pc += 2;
                    ROL(zeroPagedAddress());
                    break;
                case 0x28:
                    pc += 1;
                    PLP();
                    break;
                case 0x29:
                    pc += 2;
                    AND(immediateValue());
                    break;
                case 0x2a:
                    pc += 1;
                    ROL();
                    break;
                case 0x2c:
                    pc += 3;
                    BIT(memory[absoluteAddress()]);
                    break;
                case 0x2d:
                    pc += 3;
                    AND(memory[absoluteAddress()]);
                    break;
                case 0x2e:
                    pc += 3;
                    ROL(absoluteAddress());
                    break;

                case 0x30:
                    BMI(relativeValue());
                    break;
                case 0x31:
                    pc += 2;
                    AND(memory[indirectIndexedAddress()]);
                    break;
                case 0x35:
                    pc += 2;
                    AND(memory[zeroPagedIndexedXAddress()]);
                    break;
                case 0x36:
                    pc += 2;
                    ROL(zeroPagedAddress());
                    break;
                case 0x38:
                    pc += 1;
                    SEC();
                    break;
                case 0x39:
                    pc += 3;
                    AND(memory[absoluteIndexedY()]);
                    break;
                case 0x3d:
                    pc += 3;
                    AND(memory[absoluteIndexedX()]);
                    break;
                case 0x3e:
                    pc += 3;
                    ROL(memory[absoluteIndexedX()]);
                    break;

                case 0x40:
                    RTI();
                    break;
                case 0x41:
                    pc += 2;
                    EOR(memory[indexedIndirectAddress()]);
                    break;
                case 0x45:
                    pc += 2;
                    EOR(memory[zeroPagedAddress()]);
                    break;
                case 0x46:
                    pc += 2;
                    LSR(zeroPagedAddress());
                    break;
                case 0x48:
                    pc += 1;
                    PHA();
                    break;
                case 0x49:
                    pc += 2;
                    EOR(immediateValue());
                    break;
                case 0x4a:
                    pc += 1;
                    LSR();
                    break;
                case 0x4c:
                    JMP(absoluteAddress());
                    break;
                case 0x4d:
                    pc += 3;
                    EOR(memory[absoluteAddress()]);
                    break;
                case 0x4e:
                    pc += 3;
                    LSR(absoluteAddress());
                    break;
                
                case 0x50:
                    BVC(relativeValue());
                    break;
                case 0x51:
                    pc += 2;
                    EOR(memory[indirectIndexedAddress()]);
                    break;
                case 0x55:
                    pc += 2;
                    EOR(memory[zeroPagedIndexedXAddress()]);
                    break;
                case 0x56:
                    pc += 2;
                    LSR(zeroPagedIndexedXAddress());
                    break;
                case 0x58:
                    pc += 1;
                    CLI();
                    break;
                case 0x59:
                    pc += 3;
                    EOR(memory[absoluteIndexedY()]);
                    break;
                case 0x5d:
                    pc += 3;
                    EOR(memory[absoluteIndexedX()]);
                    break;
                case 0x5e:
                    pc += 3;
                    LSR(absoluteIndexedX());
                    break;
                
                case 0x60:
                    RTS();
                    break;
                case 0x61:
                    pc += 2;
                    ADC(memory[indexedIndirectAddress()]);
                    break;
                case 0x65:
                    pc += 2;
                    ADC(memory[zeroPagedAddress()]);
                    break;
                case 0x66:
                    pc += 2;
                    ROR(zeroPagedAddress());
                    break;
                case 0x68:
                    pc +=2;
                    PLA();
                    break;
                case 0x69:
                    pc += 2;
                    ADC(immediateValue());
                    break;
                case 0x6a:
                    pc += 2;
                    ROR();
                    break;
                case 0x6c:
                    JMP(indirectAbsoluteAddress());
                    break;
                case 0x6d:
                    pc += 3;
                    ADC(memory[absoluteAddress()]);
                    break;
                case 0x6e:
                    pc += 3;
                    ROR(absoluteAddress());
                    break;
                
                case 0x70:
                    pc += 2;
                    BVS(relativeValue());
                    break;
                case 0x71:
                    pc += 2;
                    ADC(memory[indirectIndexedAddress()]);
                    break;                
                case 0x75:
                    pc += 2;
                    ADC(memory[zeroPagedIndexedXAddress()]);
                    break;
                case 0x76:
                    pc += 2;
                    ROR(zeroPagedIndexedXAddress());
                    break;
                case 0x7d:
                    pc += 3;
                    ADC(memory[absoluteIndexedX()]);
                    break;
                case 0x7e:
                    pc += 3;
                    ROR(memory[absoluteIndexedX()]);
                    break;

                case 0x81:
                    pc += 2;
                    STA(indexedIndirectAddress());
                    break;
                case 0x84:
                    pc += 2;
                    STY(zeroPagedAddress());
                    break;
                case 0x85:
                    pc += 2;
                    STA(zeroPagedAddress());
                    break;
                case 0x86:
                    pc += 2;
                    STX(zeroPagedAddress());
                    break;
                case 0x88:
                    pc += 1;
                    DEY();
                    break;
                case 0x8a:
                    pc += 1;
                    TXA();
                    break;
                case 0x8c:
                    pc += 3;
                    STY(absoluteAddress());
                    break;
                case 0x8d:
                    pc += 3;
                    STA(absoluteAddress());
                    break;
                case 0x8e:
                    pc += 3;
                    STX(absoluteAddress());
                    break;

                case 0x90:
                    pc += 2;
                    BCC(relativeValue());
                    break;
                case 0x91:
                    pc += 2;
                    STA(indexedIndirectAddress());
                    break;
                case 0x94:
                    pc += 2;
                    STY(zeroPagedIndexedXAddress());
                    break;
                case 0x95:
                    pc += 2;
                    STA(zeroPagedIndexedXAddress());
                    break;
                case 0x96:
                    pc += 2;
                    STX(zeroPagedIndexedYAddress());
                    break;
                case 0x98:
                    pc += 1;
                    TYA();
                    break;
                case 0x99:
                    pc += 3;
                    STA(absoluteIndexedY());
                    break;
                case 0x9a:
                    pc += 1;
                    TXS();
                    break;
                case 0x9d:
                    pc += 3;
                    STA(absoluteIndexedX());
                    break;

                default:
                    pc += 1;
                    NOP();
                    break;
            }
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
};

int main() {
    return 0;
}