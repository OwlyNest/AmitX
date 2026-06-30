#!/usr/bin/env python3
import sys
import tokenize
import struct
from pathlib import Path
from dataclasses import dataclass
from keystone import Ks, KS_ARCH_X86, KS_MODE_32

ks = Ks(KS_ARCH_X86, KS_MODE_32)

@dataclass
class Token:
    kind: str
    value: str
    line: int
    column: int
    
@dataclass
class Instruction:
    opcode: str
    operands: list[str]
    
@dataclass
class Label:
    name: str

@dataclass
class Directive:
    name: str
    operands: list
    label: str | None = None
    
@dataclass
class Symbol:
    name: str
    offset: int | None = None   # resolved in pass 2

@dataclass
class Relocation:
    offset: int
    type: int
    reserved: int
    symbol: str

@dataclass
class Section:
    name: str
    data: bytearray
    relocations: list[Relocation]
    symbols: dict[str, int]

@dataclass
class AMXHeader:
    version: int = 1
    flags: int = 0
    entry: int = 0
    stack_size: int = 4096
    program_name: str = ""
    author: str = ""

MNEMONICS = {"mov", "add", "sub", "jmp"}
REGISTERS = {"eax", "ebx", "ecx", "edx"}
DIRECTIVES = {"db", "dw", "dd", "section", "global", "extern"}

import re

def load_defines(path):
    defines = {}

    define_re = re.compile(
        r'^\s*#define\s+([A-Za-z_][A-Za-z0-9_]*)\s+(.+?)\s*$'
    )

    with open(path) as f:
        for line in f:
            line = line.split("//")[0].split("/*")[0].strip()

            m = define_re.match(line)
            if not m:
                continue

            name, value = m.groups()

            try:
                defines[name] = int(value, 0)
            except ValueError:
                # Skip non-numeric macros
                pass

    return defines

class AMX_LEXER:
    def __init__(self, text):
        self.text = text
        self.pos = 0
        self.line = 1
        self.column = 1

    def current(self):
        if self.pos >= len(self.text):
            return None
        return self.text[self.pos]

    def advance(self):
        if self.current() == "\n":
            self.line += 1
            self.column = 1
        else:
            self.column += 1

        self.pos += 1
    
    def identifier(self):
        start = self.column
        value = ""

        while (c := self.current()) is not None and (c.isalnum() or c == "_"):
            value += c
            self.advance()

        return Token("IDENT", value, self.line, start)
    
    def string(self):
        self.advance()  # skip opening quote
        value = ""

        while self.current() is not None and self.current() != '"':
            value += self.current()
            self.advance()

        if self.current() != '"':
            raise SyntaxError("Unterminated string")

        self.advance()  # skip closing quote
        return Token("STRING", value, self.line, self.column)
    
    def number(self):
        start = self.column
        value = ""

        while (c := self.current()) is not None and c.isdigit():
            value += c
            self.advance()
            
        if value == "0" and self.current() in ("x", "X"):
            value += self.current()
            self.advance()

            while (c := self.current()) is not None and c in "0123456789abcdefABCDEF":
                value += c
                self.advance()

        return Token("NUMBER", value, self.line, start)
    
    def lex(self):
        tokens = []

        while (c := self.current()) is not None:

            if c in " \t":
                self.advance()

            elif c == "\n":
                tokens.append(Token("NEWLINE", "\\n", self.line, self.column))
                self.advance()

            elif c.isalpha() or c == "_":
                tokens.append(self.identifier())

            elif c.isdigit():
                tokens.append(self.number())

            elif c == ",":
                tokens.append(Token("COMMA", ",", self.line, self.column))
                self.advance()

            elif c == ":":
                tokens.append(Token("COLON", ":", self.line, self.column))
                self.advance()
                
            elif c == "\"":
                tokens.append(self.string())
                self.advance()

            else:
                raise SyntaxError(
                    f"Unexpected character '{c}' at {self.line}:{self.column}"
                )

        tokens.append(Token("EOF", "", self.line, self.column))
        return tokens

class AMX_PARSER:
    def __init__(self, tokens):
        self.tokens = tokens
        self.pos = 0

    def current(self):
        return self.tokens[self.pos]

    def peek(self, offset=1):
        pos = self.pos + offset
        if pos >= len(self.tokens):
            return self.tokens[-1]
        return self.tokens[pos]

    def advance(self):
        self.pos += 1

    def expect(self, kind):
        tok = self.current()

        if tok.kind != kind:
            raise SyntaxError(
                f"Expected {kind}, got {tok.kind} "
                f"at {tok.line}:{tok.column}"
            )

        self.advance()
        return tok

    def parse_operands(self):
        operands = []

        while self.current().kind not in ("NEWLINE", "EOF"):

            tok = self.current()

            if tok.kind == "COMMA":
                self.advance()
                continue

            operands.append(tok.value)
            self.advance()

        return operands

    def parse_instruction(self):
        opcode = self.expect("IDENT").value.upper()
        operands = self.parse_operands()

        if self.current().kind == "NEWLINE":
            self.advance()

        return Instruction(opcode, operands)

    def parse_directive(self, label=None):
        name = self.expect("IDENT").value.lower()
        operands = self.parse_operands()

        if self.current().kind == "NEWLINE":
            self.advance()

        return Directive(name, operands, label)

    def parse_label(self):
        label = self.expect("IDENT").value
        self.expect("COLON")

        # label on its own
        if self.current().kind == "NEWLINE":
            self.advance()
            return Label(label)

        # label followed by something
        if self.current().kind != "IDENT":
            raise SyntaxError(
                f"Expected instruction or directive after label '{label}'"
            )

        word = self.current().value.lower()

        if word in DIRECTIVES:
            return self.parse_directive(label)

        inst = self.parse_instruction()

        return (Label(label), inst)

    def parse_statement(self):
        if self.current().kind == "NEWLINE":
            self.advance()
            return None

        # label?
        if (self.current().kind == "IDENT" and self.peek().kind == "COLON"):
            return self.parse_label()

        word = self.current().value.lower()

        if word in DIRECTIVES:
            return self.parse_directive()

        return self.parse_instruction()

    def parse(self):
        program = []

        while self.current().kind != "EOF":
            stmt = self.parse_statement()

            if stmt is None:
                continue

            if isinstance(stmt, tuple):
                program.extend(stmt)
            else:
                program.append(stmt)

        return program
    
    
    
def assemble_pass1(program):
    text = Section(".text", bytearray(), [], {})
    offset = 0

    for stmt in program:
        if isinstance(stmt, Instruction):
            if stmt.opcode == "MOV" and stmt.operands[1].isalpha():
                # symbol relocation case
                encoding, _ = ks.asm(f"mov ebx, 0")
                text.data.extend(encoding)

                text.relocations.append(Relocation(
                    offset=offset + 1,  # where imm32 begins
                    type=defines["AMX_RELOC_ABS32"],
                    reserved=0,
                    symbol=stmt.operands[1]
                ))

            else:
                encoding, _ = ks.asm(
                    f"{stmt.opcode.lower()} {', '.join(stmt.operands)}"
                )
                text.data.extend(encoding)

            offset = len(text.data)

        elif isinstance(stmt, Label):
            text.symbols[stmt.name] = offset
            
        elif isinstance(stmt, Directive):

            if stmt.label is not None:
                text.symbols[stmt.label] = offset

            if stmt.name == "db":
                for operand in stmt.operands:
                    if isinstance(operand, str):
                        if operand.isdigit():
                            text.data.append(int(operand))
                        else:
                            text.data.extend(operand.encode("ascii"))

                offset = len(text.data)

    return text


def resolve_symbols(section):
    for rel in section.relocations:
        if rel.symbol not in section.symbols:
            raise Exception("Unknown symbol")

        addr = section.symbols[rel.symbol]

        struct.pack_into(
            "<I",
            section.data,
            rel.offset,
            addr
        )
        
def build_amx(section, header):
    image_offset = 104

    reloc_offset = image_offset + len(section.data)

    header.image_offset = image_offset
    header.image_size = len(section.data)
    header.reloc_offset = reloc_offset
    header.reloc_count = len(section.relocations)
    
def build_image(section):
    return bytes(section.data)

def build_relocations(section):
    out = bytearray()

    for rel in section.relocations:
        out.extend(struct.pack(
            "<IHH",
            rel.offset,
            rel.type,
            rel.reserved
        ))

    return bytes(out)

def build_header(section):
    image = build_image(section)
    relocs = build_relocations(section)

    image_offset = 104
    reloc_offset = image_offset + len(image)

    return struct.pack(
        "<4sHHIIIII II32s32sI",

        b"AMX\0",
        defines["AMX_VERSION"],
        0,

        image_offset,
        len(image),
        0,                      # entry
        0,                      # bss
        4096,                   # stack

        reloc_offset,
        len(section.relocations),

        b"hello\0",
        b"amity\0",

        0                       # checksum
    )

if __name__ == "__main__":
    defines = {}
    defines.update(load_defines("include/exec/amx.h"))
    defines.update(load_defines("include/kernel/syscall.h"))

    print(defines["AMX_RELOC_ABS32"])
    print(defines["SYS_PUTS"])
    
    input = sys.argv[1]
    
    with open(input, "r", encoding="utf-8") as f:
        source = f.read()

    lexer = AMX_LEXER(source)
    tokens = lexer.lex()
        
    parser = AMX_PARSER(tokens)
    program = parser.parse()
        
    text = assemble_pass1(program)
    resolve_symbols(text)
    
    header = build_header(text)
    image  = build_image(text)
    relocs = build_relocations(text)
    
    input_path = Path(input)
    output_path = input_path.with_suffix(".AMX")

    with open(output_path, "wb") as f:
        f.write(header)
        f.write(image)
        f.write(relocs)

    print(f"Wrote {output_path}")