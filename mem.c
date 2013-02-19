/*

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/
#include "mem.h"
#include <string.h>
#include <stdlib.h>
#include "error.h"
#include "file.h"

static struct {       //Linear memory dump
    size_t p, len;
    uint8_t *data;
} mem;

struct memblock_s { //starts and sizes
    size_t p, len;
    address_t start;
};

static struct {
    unsigned int p, len;
    struct memblock_s *data;
} memblocks;

static size_t memblocklastp = 0;
static address_t memblocklaststart = 0;

static int memblockcomp(const void *a, const void *b) {
    const struct memblock_s *aa=(struct memblock_s *)a;
    const struct memblock_s *bb=(struct memblock_s *)b;
    return aa->start-bb->start;
}

void memcomp(void) {
    unsigned int i, j, k;
    memjmp(0);
    if (memblocks.p<2) return;

    for (k = j = 0; j < memblocks.p; j++) {
        struct memblock_s *bj = &memblocks.data[j];
        if (bj->len) {
            for (i = j + 1; i < memblocks.p; i++) if (memblocks.data[i].len) {
                struct memblock_s *bi = &memblocks.data[i];
                if (bj->start <= bi->start && (bj->start + bj->len) > bi->start) {
                    size_t overlap = (bj->start + bj->len) - bi->start;
                    if (overlap > bi->len) overlap = bi->len;
                    memcpy(mem.data + bj->p + (unsigned)(bi->start - bj->start), mem.data + bi->p, overlap);
                    bi->len-=overlap;
                    bi->p+=overlap;
                    bi->start+=overlap;
                    continue;
                }
                if (bi->start <= bj->start && (bi->start + bi->len) > bj->start) {
                    size_t overlap = bi->start + bi->len - bj->start;
                    if (overlap > bj->len) overlap = bj->len;
                    bj->start+=overlap;
                    bj->p+=overlap;
                    bj->len-=overlap;
                    if (!bj->len) break;
                }
            }
            if (bj->len) {
                if (j!=k) memblocks.data[k]=*bj;
                k++;
            }
        }
    }
    memblocks.p = k;
    qsort(memblocks.data, memblocks.p, sizeof(*memblocks.data), memblockcomp);
}

void memjmp(address_t adr) {
    if (mem.p == memblocklastp) {
        memblocklaststart = adr;
        return;
    }
    if (memblocks.p>=memblocks.len) {
        memblocks.len+=64;
        memblocks.data=realloc(memblocks.data, memblocks.len*sizeof(*memblocks.data));
        if (!memblocks.data) err_msg_out_of_memory();
    }
    memblocks.data[memblocks.p].len = mem.p-memblocklastp;
    memblocks.data[memblocks.p].p = memblocklastp;
    memblocks.data[memblocks.p++].start = memblocklaststart;
    memblocklastp = mem.p;
    memblocklaststart = adr;
}

void memprint(void) {
    char temp[10];
    unsigned int i;
    address_t start, end;

    if (memblocks.p) {
        start = memblocks.data[0].start;
        end = memblocks.data[0].start + memblocks.data[0].len;
        for (i=1;i<memblocks.p;i++) {
            if (memblocks.data[i].start != end) {
                sprintf(temp, "$%04" PRIaddress, start);
                printf("Memory range:    %7s-$%04" PRIaddress "\n", temp, end-1);
                start = memblocks.data[i].start;
            }
            end = memblocks.data[i].start + memblocks.data[i].len;
        }
        sprintf(temp, "$%04" PRIaddress, start);
        printf("Memory range:    %7s-$%04" PRIaddress "\n", temp, end-1);
    } else puts("Memory range:      None");
}

void output_mem(int scpumode) {
    address_t start;
    size_t size;
    unsigned int i, last;
    FILE* fout;

    if (mem.p) {
        if (arguments.output[0] == '-' && !arguments.output[1]) {
            fout = stdout;
        } else {
            if ((fout=file_open(arguments.output,"wb"))==NULL) err_msg_file(ERROR_CANT_WRTE_OBJ, arguments.output);
        }
        clearerr(fout);
        if (memblocks.p) {
            start = memblocks.data[0].start;
            last = 0;
            if (!arguments.nonlinear && arguments.flat) {
                size = start;
                while (size--) putc(0, fout);
            }
            size = memblocks.data[0].len;
            for (i=1;i<memblocks.p;i++) {
                if (memblocks.data[i].start != start + size) {
                    if (arguments.nonlinear) {
                        putc(size,fout);
                        putc(size >> 8,fout);
                        if (scpumode) putc(size >> 16,fout);
                    }
                    if ((!arguments.stripstart && !arguments.flat && !last) || arguments.nonlinear) {
                        putc(start,fout);
                        putc(start >> 8,fout);
                        if (scpumode && (!arguments.wordstart || arguments.nonlinear)) putc(start >> 16,fout);
                    }
                    while (last<i) {
                        fwrite(mem.data+memblocks.data[last].p,memblocks.data[last].len,1,fout);
                        last++;
                    }
                    if (!arguments.nonlinear) {
                        size = memblocks.data[i].start - start - size;
                        while (size--) putc(0, fout);
                    }
                    start = memblocks.data[i].start;
                    size = 0;
                }
                size += memblocks.data[i].len;
            }
            if (arguments.nonlinear) {
                putc(size,fout);
                putc(size >> 8,fout);
                if (scpumode) putc(size >> 16,fout);
            }
            if ((!arguments.stripstart && !arguments.flat && !last) || arguments.nonlinear) {
                putc(start,fout);
                putc(start >> 8,fout);
                if (scpumode && (!arguments.wordstart || arguments.nonlinear)) putc(start >> 16,fout);
            }
            while (last<i) {
                fwrite(mem.data+memblocks.data[last].p,memblocks.data[last].len,1,fout);
                last++;
            }
        }
        if (arguments.nonlinear) {
            putc(0,fout);
            putc(0,fout);
            if (scpumode) putc(0 ,fout);
        }
        if (ferror(fout)) err_msg_file(ERROR_CANT_WRTE_OBJ, arguments.output);
        if (fout != stdout) fclose(fout);
    }
}

void write_mem(uint8_t c) {
    if (mem.p>=mem.len) {
        mem.len+=0x1000;
        mem.data=realloc(mem.data, mem.len);
        if (!mem.data) err_msg_out_of_memory();
    }
    mem.data[mem.p++] = c;
}

static unsigned int omemp;
static size_t ptextaddr;
static address_t oaddr;

void mark_mem(address_t adr) {
    ptextaddr=mem.p;
    omemp = memblocks.p;
    oaddr = adr;
}

void write_mark_mem(uint8_t c) {
    mem.data[ptextaddr] = c;
}


extern void printllist(FILE *);

void list_mem(FILE *flist, address_t all_mem, const uint8_t **llist, int dooutput, enum lastl_e *lastl) { 
    unsigned int i, lcol;
    address_t myaddr;
    size_t len;
    for (;omemp <= memblocks.p;omemp++) {
        lcol=arguments.source?25:49;
        if (omemp < memblocks.p) {
            len = memblocks.data[omemp].len - (ptextaddr - memblocks.data[omemp].p);
            myaddr = (memblocks.data[omemp].start + memblocks.data[omemp].len - len) & all_mem;
        } else {
            myaddr = memblocklaststart + (ptextaddr - memblocklastp);
            len = mem.p - ptextaddr;
            if (!len) {
                if (!*llist) continue;
                if (omemp) myaddr = (memblocks.data[omemp-1].start + memblocks.data[omemp-1].len) & all_mem;
                else myaddr = oaddr;
            }
        }
        if (*lastl != LIST_DATA) {putc('\n',flist);*lastl = LIST_DATA;}
        if (dooutput) {
            fprintf(flist,(all_mem==0xffff)?">%04" PRIaddress "\t":">%06" PRIaddress " ", myaddr);
            while (len) {
                if (lcol==1) {
                    if (arguments.source && *llist) {
                        putc('\t', flist);printllist(flist);
                    } else putc('\n',flist);
                    fprintf(flist,(all_mem==0xffff)?">%04" PRIaddress "\t":">%06" PRIaddress " ", myaddr);lcol=49;
                }
                fprintf(flist," %02x", mem.data[ptextaddr++]);
                myaddr = (myaddr + 1) & all_mem;

                lcol-=3;
                len--;
            }
        } else fprintf(flist,(all_mem==0xffff)?">%04" PRIaddress "\t":">%06" PRIaddress " ", oaddr);

        if (arguments.source && *llist) {
            for (i=0; i<lcol-1; i+=8) putc('\t',flist);
            putc('\t', flist);printllist(flist);
        } else putc('\n',flist);
    }
}

void restart_mem(void) {
    mem.p=0;memblocklastp=0;memblocks.p=0;memblocklaststart=0;
}

void init_mem(void) {
    mem.p = 0;
    mem.len = 0;
    mem.data = NULL;
    memblocks.p = 0;
    memblocks.len = 0;
    memblocks.data = NULL;
}

void destroy_mem(void) {
    free(mem.data);
    free(memblocks.data);
}
