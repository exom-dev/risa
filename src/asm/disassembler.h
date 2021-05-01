#ifndef RISA_DISASSEMBLER_H_GUARD
#define RISA_DISASSEMBLER_H_GUARD

#include "../io/io.h"
#include "../cluster/cluster.h"
#include "../def/types.h"

typedef struct {
    RisaIO io;
    RisaCluster* cluster;
    size_t offset;
} RisaDisassembler;

void risa_disassembler_init  (RisaDisassembler* disassembler);
void risa_disassembler_load  (RisaDisassembler* disassembler, RisaCluster* chunk);
void risa_disassembler_run   (RisaDisassembler* disassembler);
void risa_disassembler_reset (RisaDisassembler* disassembler);

#endif
