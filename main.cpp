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

        void reset() {
            pc = ((uint16_t)memory[RESET-1] << 8) | memory[RESET];
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
                    ORA(memory[indirectIndexedAddress()]);
                    break;
                case 0x05:
                    ORA(memory[zeroPagedAddress()]);
                    break;
                case 0x06:
                    ASL(memory[pc+1]);
                    break;
                case 0x08:
                    PHP();
                    break;
                case 0x09:
                    ORA(memory[pc+1]);
                    break;
                case 0x0a:
                    ASL();
                    break;
                case 0x0d:
                    ORA(memory[absoluteAddress()]);
                    break;
                case 0x0e:
                    ASL(absoluteAddress());
                    break;

                case 0x10:
                    BPL(memory[pc+1]);
                    break;
                case 0x11:
                    ORA(memory[indirectIndexedAddress()]);
                    break;
                case 0x15:
                    ORA(memory[zeroPagedIndexedAddress()]);
                    break;
                case 0x16:
                    ASL(zeroPagedIndexedAddress());
                    break;
                case 0x18:
                    CLC();
                    break;
                case 0x19:
                    ORA(memory[absoluteIndexedY()]);
                    break;
                case 0x1d:
                    ORA(memory[absoluteIndexedX()]);
                    break;
                case 0x1e:
                    ASL(absoluteIndexedX());
                    break;

                case 0x20:
                    JSR(absoluteAddress());
                    break;
                case 0x21:
                    AND(memory[indirectIndexedAddress()]);
                    break;
                case 0x24:
                    BIT(memory[zeroPagedAddress()]);
                    break;
                case 0x25:
                    AND(memory[zeroPagedIndexedAddress()]);
                    break;
                case 0x26:
                    ROL(zeroPagedAddress());
                    break;
                case 0x28:
                    PLP();
                    break;
                case 0x29:
                    AND(memory[pc+1]);
                    break;
                case 0x2a:
                    ROL();
                    break;
                case 0x2c:
                    BIT(memory[absoluteAddress()]);
                    break;
                case 0x2d:
                    AND(memory[absoluteAddress()]);
                    break;
                case 0x2e:
                    ROL(absoluteAddress());
                    break;

                case 0x31:
                    BMI(memory[pc+1]);
                    break;
                case 0x32:
                    AND(memory[indirectIndexedAddress()]);
                    break;
                case 0x35:
                    AND(memory[zeroPagedIndexedAddress()]);
                    break;
                case 0x36:
                    ROL(zeroPagedAddress());
                    break;
                case 0x38:
                    SEC();
                    break;
                case 0x39:
                    AND(memory[absoluteIndexedY()]);
                    break;
                case 0x3d:
                    AND(memory[absoluteIndexedX()]);
                    break;
                case 0x3e:
                    ROL(memory[absoluteAddress()]);
                    break;
            }
        }

        uint16_t absoluteAddress() {
            return ((uint16_t)memory[pc+1] << 8) | memory[pc+2];
        }

        uint16_t absoluteIndexedY() {
            return absoluteAddress()+y;
        }

        uint16_t absoluteIndexedX() {
            return absoluteAddress()+x;
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

        uint8_t zeroPagedIndexedAddress() {
            return memory[pc+1]+x;
        }

        uint8_t zeroPagedAddress() {
            return memory[pc+1];
        }
        
        void executeIRQ() {
            pc = ((uint16_t)memory[IRQ-1] << 8) | memory[IRQ];

            setFlag(INTERRUPT_FLAG);

            isIRQ = false;
        }

        void executeNMI() {
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
        
        // instructions

        void BRK() {
            pc = ((uint16_t)memory[IRQ-1] << 8) | memory[IRQ];

            setFlag(BREAK_FLAG);
            setFlag(INTERRUPT_FLAG);
        }

        void ORA(uint8_t operand) {
            pc += 2;

            accumulator |= operand;

            // Negative flag
            if (accumulator & 0x80 > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            // Zero flag    
            if (accumulator == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void ASL(uint8_t operand) {
            pc += 2;

            if ((memory[operand] & 0x80) > 0) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);

            memory[operand] <<= 1;

            if (memory[operand] == 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if ((memory[operand] & 0x80) > 0) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);
        }

        void ASL(uint16_t operand) {
            pc += 3;

            if ((memory[operand] & 0x80) > 0) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);

            memory[operand] <<= 1;

            if (memory[operand] == 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if ((memory[operand] & 0x80) > 0) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);
        }

        void ASL() {
            pc += 1;

            if ((accumulator & 0x80) > 0) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);

            accumulator <<= 1;

            if ((accumulator & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (accumulator == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void PHP() {
            pc += 1;
            pushStack(psr);   
        }

        void BPL(uint8_t operand) {
            if (!checkFlag(NEGATIVE_FLAG)) pc += (int8_t)operand;
            else pc += 2;
        }

        void CLC() {
            pc += 1;
            unsetFlag(CARRY_FLAG);
        }

        void JSR(uint16_t operand) {
            pc += 3;

            uint8_t pc_high = pc >> 8;
            uint8_t pc_low = pc;

            pushStack(pc_low);
            pushStack(pc_high);
        }

        void AND(uint8_t operand) {
            pc += 2;
            
            accumulator &= operand;

            if ((accumulator & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (accumulator == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void BIT(uint8_t operand) {
            pc += 2;

            uint8_t temp = accumulator & operand;
            
            if ((temp & 0x40) > 0) setFlag(OVERFLOW_FLAG);
            else unsetFlag(OVERFLOW_FLAG);

            if ((temp & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (temp == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void ROL(uint8_t operand) {
            pc += 2;

            if ((memory[operand] & 0x80) > 0) setFlag(CARRY_FLAG);
            else unsetFlag(CARRY_FLAG);

            uint8_t temp = (0x80 & memory[operand]) >> 7;

            memory[operand] = (memory[operand] << 1) | temp;

            if ((memory[operand] & 0x80) > 0) setFlag(NEGATIVE_FLAG);
            else unsetFlag(NEGATIVE_FLAG);

            if (memory[operand] == 0) setFlag(ZERO_FLAG);
            else unsetFlag(ZERO_FLAG);
        }

        void ROL() {
            pc += 1;

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
            pc += 1;
            psr = pullStack();
        }
        
        void BMI(uint8_t operand) {
            if (checkFlag(NEGATIVE_FLAG)) pc += (int8_t)operand;
            else pc += 2;
        }

        void SEC() {
            pc += 1;
            setFlag(CARRY_FLAG);
        }
};

int main() {
    return 0;
}