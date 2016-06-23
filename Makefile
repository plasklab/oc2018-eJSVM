CC = clang
CFLAGS = -std=gnu89 -Wall -g -DUSER_DEF
LIBS = -lc -lm

GENERATED_HFILES = \
         instructions-opcode.h \
	 instructions-table.h \
	 instructions-label.h

HFILES = $(GENERATED_HFILES) \
         prefix.h \
         context.h \
         header.h \
	 builtin.h \
	 hash.h \
	 instructions.h \
	 types.h \
	 globals.h \
	 extern.h

OFILES = allocate.o \
	 builtin.o \
	 builtin-array.o \
	 builtin-boolean.o \
	 builtin-global.o \
	 builtin-math.o \
	 builtin-number.o \
	 builtin-object.o \
	 builtin-regexp.o \
	 builtin-string.o \
	 call.o \
         codeloader.o \
         context.o \
	 conversion.o \
	 hash.o \
	 init.o \
	 object.o \
	 operations.o \
	 vmloop.o \
	 main.o

SEDCOM_GEN_INSN_OPCODE = 's/^\([a-z][a-z]*\).*/\U\1,/'
SEDCOM_GEN_INSN_TABLE  = 's/^\([a-z][a-z]*\)  *\([A-Z][A-Z]*\).*/  { "\1", \2 },/'
SEDCOM_GEN_INSN_LABEL  = 's/^\([a-z][a-z]*\).*/\&\&I_\U\1,/'
SED = gsed

ssjsvm :: $(OFILES)
	$(CC) -o $@ $^ $(LIBS)

instructions-opcode.h: instructions.def
	$(SED) -e $(SEDCOM_GEN_INSN_OPCODE) instructions.def > $@

instructions-table.h: instructions.def
	$(SED) -e $(SEDCOM_GEN_INSN_TABLE) instructions.def > $@

instructions-label.h: instructions.def
	$(SED) -e $(SEDCOM_GEN_INSN_LABEL) instructions.def > $@

instructions.h: instructions-opcode.h instructions-table.h

%.o: %.c $(HFILES)
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm $(OFILES) $(GENERATED_HFILES)
