/* apploader image / ELF32 PowerPC converter. ISO C99, no external libraries. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SECTIONS 64
#define MAX_FILE (128u * 1024u * 1024u)
typedef struct {
  unsigned char* data;
  size_t size;
} Buffer;
typedef struct {
  char name[64];
  uint32_t offset, address, size, type, flags, align, nameoff, elfoff;
} Section;
typedef struct {
  int trailer;
  int dol;
  uint32_t entry;
  size_t count;
  Section sections[MAX_SECTIONS];
} Layout;
static void fail(const char* s) {
  fprintf(stderr, "apploader: %s\n", s);
  exit(1);
}

static uint32_t be32(const unsigned char* p) {
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static unsigned be16(const unsigned char* p) { return (unsigned)p[0] << 8 | p[1]; }
static void put32(unsigned char* p, uint32_t n) {
  p[0] = (unsigned char)(n >> 24);
  p[1] = (unsigned char)(n >> 16);
  p[2] = (unsigned char)(n >> 8);
  p[3] = (unsigned char)n;
}

static void put16(unsigned char* p, unsigned n) {
  p[0] = (unsigned char)(n >> 8);
  p[1] = (unsigned char)n;
}

static void range(size_t off, size_t len, size_t total) {
  if (off > total || len > total - off)
    fail("range outside file");
}

static uint32_t alignup(uint32_t n, uint32_t a) {
  if (!a || (a & (a - 1)) || n > UINT32_MAX - (a - 1))
    fail("invalid alignment");
  return (n + a - 1) & ~(a - 1);
}

static Buffer readfile(const char* path) {
  Buffer b;
  long n;
  FILE* f = fopen(path, "rb");
  if (!f)
    fail(path);
  if (fseek(f, 0, SEEK_END) || (n = ftell(f)) < 0 || n > MAX_FILE || fseek(f, 0, SEEK_SET))
    fail("invalid file size");
  b.size = (size_t)n;
  b.data = calloc(b.size + 1, 1);
  if (!b.data)
    fail("out of memory");
  if (fread(b.data, 1, b.size, f) != b.size || fclose(f))
    fail("read failed");
  return b;
}

static void writefile(const char* path, Buffer b) {
  FILE* f = fopen(path, "wb");
  if (!f)
    fail(path);
  if (fwrite(b.data, 1, b.size, f) != b.size || fclose(f))
    fail("write failed");
}

static void header(Buffer b) {
  if (b.size < 32)
    fail("truncated apploader header");
  range(32, be32(b.data + 20), b.size);
  range(32 + (size_t)be32(b.data + 20), be32(b.data + 24), b.size);
}

/* The boot ROM loads the loader at this platform-defined address. */
#define LOADER_ADDRESS 0x81200000u

static void add_section(Layout* l, const char* name, uint32_t offset, uint32_t address,
                        uint32_t size, uint32_t type, uint32_t flags, uint32_t align) {
  Section* s;
  size_t i;
  if (!size || l->count >= MAX_SECTIONS - 4 || strlen(name) >= sizeof(l->sections[0].name))
    fail("invalid section");
  if ((uint64_t)address + size > UINT32_MAX)
    fail("section address overflow");
  for (i = 0; i < l->count; ++i) {
    Section* other = &l->sections[i];
    if (!strcmp(name, other->name) || ((uint64_t)address + size > other->address &&
                                       (uint64_t)other->address + other->size > address))
      fail("overlapping or duplicate sections");
  }
  s = &l->sections[l->count++];
  strcpy(s->name, name);
  s->offset = offset;
  s->address = address;
  s->size = size;
  s->type = type;
  s->flags = flags;
  s->align = align;
}

static int address_pair(uint32_t high, uint32_t low, uint32_t* address) {
  unsigned reg = (high >> 21) & 31;
  if ((high & 0xfc1f0000) != 0x3c000000)
    return 0;
  if ((low >> 26) == 14 && ((low >> 16) & 31) == reg) {
    *address = (high << 16) + (int16_t)low;
    return 1;
  }
  if ((low >> 26) == 24 && ((low >> 21) & 31) == reg) {
    *address = (high << 16) | (low & 0xffff);
    return 1;
  }
  return 0;
}

static int is_string(const unsigned char* data, size_t size, size_t offset) {
  size_t start = offset;
  while (offset < size && data[offset]) {
    unsigned c = data[offset++];
    if ((c < 32 || c > 126) && c != '\n' && c != '\r' && c != '\t')
      return 0;
  }
  return offset < size && offset - start >= 16;
}

static int writes_register(uint32_t word, unsigned reg) {
  unsigned op = word >> 26, rt = (word >> 21) & 31, ra = (word >> 16) & 31;
  if (op == 18 && (word & 1))
    return reg <= 12; /* A call may destroy volatile registers. */
  if (op == 14 || op == 15 || (op >= 32 && op <= 35) || (op >= 40 && op <= 43))
    return rt == reg;
  if (op == 20 || op == 21 || (op >= 24 && op <= 29))
    return ra == reg;
  if (op == 31)
    return rt == reg || ra == reg;
  return 0;
}

static uint32_t zero_end(const unsigned char* body, uint32_t size, uint32_t start) {
  while (start < size && !body[start])
    ++start;
  return start;
}

static uint32_t memory_access_size(uint32_t word) {
  unsigned op = word >> 26;
  if (op < 32 || op > 55)
    return 0;
  if (op == 46 || op == 47)
    return (32 - ((word >> 21) & 31)) * 4;
  if (op >= 48)
    return (op & 2) ? 8 : 4;
  if (op >= 40)
    return 2;
  return (op & 2) ? 1 : 4;
}

static Layout loader_layout(Buffer image) {
  Layout l = {0};
  const unsigned char* body = image.data + 32;
  uint32_t size = be32(image.data + 20), strings = size, bss = size, bssEnd = size;
  uint32_t codeEnd = 0, data, i, j, address, largest = 0;
  unsigned char* referenced = calloc(size ? size : 1, 1);
  if (!referenced)
    fail("out of memory");
  l.entry = be32(image.data + 16);
  if (l.entry < LOADER_ADDRESS || (uint64_t)l.entry >= (uint64_t)LOADER_ADDRESS + size)
    fail("loader entry is outside the boot ROM load range");

  /* Scheduled code can separate lis from its addi/ori. Follow the high half
   * until the register is overwritten instead of requiring adjacent words. */
  for (i = 0; i + 8 <= size; i += 4) {
    uint32_t high = be32(body + i);
    unsigned reg = (high >> 21) & 31;
    if ((high & 0xfc1f0000) != 0x3c000000)
      continue;
    for (j = i + 4; j + 4 <= size && j - i <= 64; j += 4) {
      uint32_t low = be32(body + j), offset;
      if (address_pair(high, low, &address) && address >= LOADER_ADDRESS) {
        offset = address - LOADER_ADDRESS;
        if (offset < size) {
          referenced[offset] = 1;
          if (offset > j && is_string(body, size, offset) && offset < strings)
            strings = offset;
        }
      }
      if (writes_register(low, reg))
        break;
    }
  }
  if (strings == size || (strings & 31))
    fail("cannot locate the loader's pooled strings");

  /* The last return before the strings bounds the contiguous code. A read-only
   * constants section can occur between the code and the string/data pool. */
  for (i = 0; i + 4 <= strings; i += 4) {
    if (be32(body + i) == 0x4e800020)
      codeEnd = i + 4;
  }
  data = alignup(codeEnd, 32);
  if (!codeEnd || data > strings || l.entry >= LOADER_ADDRESS + data)
    fail("cannot establish the loader code boundary");
  for (i = codeEnd; i < data; ++i) {
    if (body[i])
      fail("nonzero bytes in loader code alignment gap");
  }

  /* BSS is a referenced, aligned zero-filled range. Debug loaders may place
   * initialized small data after it, so it need not be the end of the image. */
  for (i = strings; i < size;) {
    uint32_t end, start;
    if (body[i]) {
      ++i;
      continue;
    }
    end = zero_end(body, size, i);
    start = alignup(i, 32);
    if (start < end && end - start > largest) {
      for (j = start; j < end; ++j) {
        if (referenced[j]) {
          bss = start;
          bssEnd = end;
          largest = end - start;
          break;
        }
      }
    }
    i = end;
  }
  free(referenced);
  if (!largest || bss <= strings)
    fail("cannot establish loader BSS from its references");

  /* SDK memory routines precede .text in .init, separated by an alignment gap. */
  for (i = 4; i + 4 <= l.entry - LOADER_ADDRESS; i += 4) {
    uint32_t next = i;
    if (be32(body + i - 4) != 0x4e800020 || be32(body + i))
      continue;
    while (next + 4 <= data && !be32(body + next))
      next += 4;
    if (next > i && next < data && !(next & 31)) {
      add_section(&l, ".init", 0, LOADER_ADDRESS, i, 1, 6, 32);
      add_section(&l, ".text", next, LOADER_ADDRESS + next, data - next, 1, 6, 32);
      break;
    }
  }
  if (!l.count)
    add_section(&l, ".text", 0, LOADER_ADDRESS, data, 1, 6, 32);
  if (strings > data)
    add_section(&l, ".rodata", data, LOADER_ADDRESS + data, strings - data, 1, 2, 32);
  add_section(&l, ".data", strings, LOADER_ADDRESS + strings, bss - strings, 1, 3, 32);
  add_section(&l, ".bss", bss, LOADER_ADDRESS + bss, bssEnd - bss, 8, 3, 32);
  if (bssEnd < size) {
    uint32_t smallBss = size, smallEnd, smallData2, referencedEnd;
    if (bssEnd & 31)
      fail("unaligned initialized data after loader BSS");
    /* EABI r13 is based 0x8000 bytes above .sdata. Its zero-filled targets
     * identify .sbss; floating-point constants follow in .sdata2. */
    for (i = 0; i + 4 <= codeEnd; i += 4) {
      uint32_t word = be32(body + i), width = memory_access_size(word);
      if (((word >> 16) & 31) == 13 && width) {
        int64_t target = (int64_t)bssEnd + 0x8000 + (int16_t)word;
        if (target >= bssEnd && target + width <= size &&
            zero_end(body, (uint32_t)target + width, (uint32_t)target) == target + width &&
            (uint32_t)target < smallBss)
          smallBss = (uint32_t)target;
      }
    }
    if (smallBss == size || (smallBss & 31))
      fail("cannot locate loader small BSS");
    smallEnd = zero_end(body, size, smallBss);
    /* A zero-valued floating-point constant can extend the zero run beyond
     * .sbss. Bound the section by the extent of its r13 accesses instead. */
    referencedEnd = smallBss;
    for (i = 0; i + 4 <= codeEnd; i += 4) {
      uint32_t word = be32(body + i), width = memory_access_size(word);
      if (((word >> 16) & 31) == 13 && width) {
        int64_t target = (int64_t)bssEnd + 0x8000 + (int16_t)word;
        if (target >= smallBss && target + width <= smallEnd && target + width > referencedEnd)
          referencedEnd = (uint32_t)target + width;
      }
    }
    smallData2 = alignup(referencedEnd, 8);
    if (smallData2 <= smallBss || smallData2 > smallEnd || smallData2 >= size)
      fail("cannot locate loader small constants");
    add_section(&l, ".sdata", bssEnd, LOADER_ADDRESS + bssEnd, smallBss - bssEnd, 1, 3, 32);
    add_section(&l, ".sbss", smallBss, LOADER_ADDRESS + smallBss, smallData2 - smallBss, 8, 3, 32);
    add_section(&l, ".sdata2", smallData2, LOADER_ADDRESS + smallData2, size - smallData2, 1, 2, 8);
  }
  return l;
}

static Layout flat_trailer_layout(const unsigned char* body, uint32_t size) {
  Layout l = {0};
  uint32_t table, base = 0, count = 0, pos, i, sdata = 0, sdata2 = 0;
  static const char* names[] = {".init",   ".text", ".ctors", ".dtors",
                                ".rodata", ".data", ".sdata", ".sdata2"};
  l.trailer = 1;
  /* Find a complete SDK ROM-copy table. Its first entry maps the image base to
   * itself, and every initialized range must fit inside the trailer. */
  for (table = 0; table + 36 <= size; table += 4) {
    uint32_t previous = 0;
    base = be32(body + table);
    if (base < 0x80000000 || base >= 0x81800000 || (base & 31) || be32(body + table + 4) != base)
      continue;
    count = 0;
    for (pos = table; pos + 12 <= size; pos += 12) {
      uint32_t source = be32(body + pos), dest = be32(body + pos + 4);
      uint32_t length = be32(body + pos + 8);
      if (!source && !dest && !length)
        break;
      if (!length || source != dest || source < base || source - base > size ||
          length > size - (source - base) || (count && source < previous) ||
          count >= MAX_SECTIONS - 8) {
        count = 0;
        break;
      }
      previous = source + length;
      ++count;
    }
    if (count >= 2 && pos + 12 <= size) {
      for (i = 0; i < count; ++i) {
        uint32_t start = be32(body + table + i * 12) - base;
        uint32_t length = be32(body + table + i * 12 + 8);
        if (start <= table && pos + 12 - start <= length)
          break;
      }
      if (i < count)
        break;
    }
  }
  if (table + 36 > size || count < 2)
    fail("cannot find a complete trailer ROM-copy table");
  l.entry = base;
  for (i = 0; i + 8 <= table; i += 4) {
    uint32_t high = be32(body + i), low = be32(body + i + 4), address;
    unsigned reg = (high >> 21) & 31;
    if (address_pair(high, low, &address)) {
      if (reg == 13)
        sdata = address - 0x8000;
      else if (reg == 2)
        sdata2 = address - 0x8000;
    }
  }
  for (i = 0; i < count; ++i) {
    uint32_t address = be32(body + table + i * 12);
    uint32_t length = be32(body + table + i * 12 + 8);
    char fallback[32];
    const char* name;
    if (address == sdata)
      name = ".sdata";
    else if (address == sdata2)
      name = ".sdata2";
    else if (count == sizeof(names) / sizeof(names[0]))
      name = names[i];
    else if (i < 2)
      name = names[i];
    else {
      snprintf(fallback, sizeof(fallback), ".data%u", i);
      name = fallback;
    }
    add_section(&l, name, address - base, address, length, 1,
                i < 2 ? 6
                      : ((!strcmp(name, ".rodata") || !strcmp(name, ".sdata2") ||
                          !strcmp(name, ".ctors") || !strcmp(name, ".dtors"))
                             ? 2
                             : 3),
                i == 0 ? 4 : 32);
  }
  pos = table + (count + 1) * 12;
  for (i = 0; pos + 8 <= size; pos += 8, ++i) {
    uint32_t address = be32(body + pos), length = be32(body + pos + 4);
    const char* name = i == 0 ? ".sbss" : ".bss";
    if (!address && !length)
      return l;
    if (i >= 2 || !length || address < base || address - base > MAX_FILE ||
        length > MAX_FILE - (address - base))
      fail("invalid trailer BSS initialization table");
    add_section(&l, name, address - base, address, length, 8, 3, 32);
  }
  fail("unterminated trailer BSS initialization table");
  return l;
}

static int is_dol(Buffer in) {
  unsigned i, text = 0;
  uint32_t entry;
  if (in.size < 256)
    return 0;
  entry = be32(in.data + 0xe0);
  for (i = 0; i < 18; ++i) {
    uint32_t offset = be32(in.data + i * 4), address = be32(in.data + 0x48 + i * 4);
    uint32_t size = be32(in.data + 0x90 + i * 4);
    if (!size)
      continue;
    if (offset < 256 || offset > in.size || size > in.size - offset || address < 0x80000000 ||
        (uint64_t)address + size > 0x81800000)
      return 0;
    if (i < 7 && entry >= address && (uint64_t)entry < (uint64_t)address + size)
      text = 1;
  }
  return text != 0;
}

static Layout trailer_layout(Buffer image) {
  Buffer dol = {image.data + 32 + be32(image.data + 20), be32(image.data + 24)};
  Layout l;
  uint32_t base = UINT32_MAX, end = 0;
  unsigned i;
  unsigned char* flat;
  if (!is_dol(dol))
    return flat_trailer_layout(dol.data, (uint32_t)dol.size);
  for (i = 0; i < 18; ++i) {
    uint32_t address = be32(dol.data + 0x48 + i * 4), size = be32(dol.data + 0x90 + i * 4);
    if (!size)
      continue;
    if (address < base)
      base = address;
    if (address + size > end)
      end = address + size;
  }
  if (end - base > MAX_FILE)
    fail("DOL memory image too large");
  flat = calloc(end - base, 1);
  if (!flat)
    fail("out of memory");
  for (i = 0; i < 18; ++i) {
    uint32_t address = be32(dol.data + 0x48 + i * 4), size = be32(dol.data + 0x90 + i * 4);
    if (size)
      memcpy(flat + address - base, dol.data + be32(dol.data + i * 4), size);
  }
  l = flat_trailer_layout(flat, end - base);
  l.dol = 1;
  l.entry = be32(dol.data + 0xe0);
  free(flat);
  for (i = 0; i < l.count; ++i) {
    Section* section = &l.sections[i];
    unsigned j;
    if (section->type == 8) {
      section->offset = (uint32_t)dol.size;
      continue;
    }
    for (j = 0; j < 18; ++j) {
      uint32_t address = be32(dol.data + 0x48 + j * 4), size = be32(dol.data + 0x90 + j * 4);
      if (size && section->address >= address && section->address - address <= size &&
          section->size <= size - (section->address - address)) {
        section->offset = be32(dol.data + j * 4) + section->address - address;
        break;
      }
    }
    if (j == 18)
      fail("ROM-copy entry is not contained in a DOL section");
  }
  return l;
}

static int compare_sections(const void* left, const void* right) {
  uint32_t a = ((const Section*)left)->address;
  uint32_t b = ((const Section*)right)->address;
  return (a > b) - (a < b);
}

static Layout detect_layout(Buffer image, const char* part) {
  Layout l;
  header(image);
  if (!strcmp(part, "loader"))
    l = loader_layout(image);
  else if (!strcmp(part, "trailer"))
    l = trailer_layout(image);
  else {
    fail("part must be loader or trailer");
    return (Layout){0};
  }
  qsort(l.sections, l.count, sizeof(l.sections[0]), compare_sections);
  return l;
}

static void toelf(const char* input, const char* spec, const char* output) {
  Buffer in = readfile(input), out;
  Layout l = detect_layout(in, spec);
  size_t i, j, n = l.count;
  uint32_t start, size, pos, shoff, stroff, strsize = 1, phcount = 0;
  unsigned char* used;
  char names[8192] = {0};
  header(in);
  start = 32 + (l.trailer ? be32(in.data + 20) : 0);
  size = be32(in.data + (l.trailer ? 24 : 20));
  used = calloc(size ? size : 1, 1);
  if (!used)
    fail("out of memory");
  if (l.dol) {
    range(0, 256, size);
    memset(used, 1, 256);
  }
  for (i = 0; i < n; i++) {
    Section* s = &l.sections[i];
    if (s->type != 8)
      range(s->offset, s->size, size);
    phcount++;
    for (j = 0; j < s->size && (uint64_t)s->offset + j < size; j++) {
      used[s->offset + j] = 1;
      if (s->type == 8 && in.data[start + s->offset + j])
        fail("nonzero data described as bss");
    }
  }
  for (i = 0; i < size; i++)
    if (!used[i] && in.data[start + i])
      fail("layout leaves nonzero bytes uncovered");
  free(used);
  if (!l.entry && !l.trailer)
    l.entry = be32(in.data + 16);
  for (i = 0; i < n; i++)
    if ((l.sections[i].flags & 4) && l.entry >= l.sections[i].address &&
        (uint64_t)l.entry < (uint64_t)l.sections[i].address + l.sections[i].size)
      break;
  if (i == n)
    fail("entry point is outside executable sections");
  pos = 52 + 32 * phcount;
  for (i = 0; i < l.count; i++) {
    Section* s = &l.sections[i];
    size_t len = strlen(s->name) + 1;
    if (strsize + len >= sizeof(names))
      fail("section names too large");
    s->nameoff = strsize;
    memcpy(names + strsize, s->name, len);
    strsize += (uint32_t)len;
    pos = alignup(pos, s->align);
    s->elfoff = pos;
    if (s->type != 8)
      pos += s->size;
  }
  stroff = pos;
  memcpy(names + strsize, ".shstrtab", 10);
  j = strsize;
  strsize += 10;
  shoff = alignup(stroff + strsize, 4);
  out.size = shoff + 40 * (l.count + 2);
  out.data = calloc(out.size, 1);
  if (!out.data)
    fail("out of memory");
  memcpy(out.data, "\177ELF\1\2\1", 7);
  put16(out.data + 16, 2);
  put16(out.data + 18, 20);
  put32(out.data + 20, 1);
  put32(out.data + 24, l.entry);
  put32(out.data + 28, 52);
  put32(out.data + 32, shoff);
  put16(out.data + 40, 52);
  put16(out.data + 42, 32);
  put16(out.data + 44, phcount);
  put16(out.data + 46, 40);
  put16(out.data + 48, (unsigned)l.count + 2);
  put16(out.data + 50, (unsigned)l.count + 1);
  for (i = 0; i < l.count; i++) {
    Section* s = &l.sections[i];
    unsigned char* sh = out.data + shoff + 40 * (i + 1);
    put32(sh, s->nameoff);
    put32(sh + 4, s->type);
    put32(sh + 8, s->flags);
    put32(sh + 12, s->address);
    put32(sh + 16, s->elfoff);
    put32(sh + 20, s->size);
    put32(sh + 32, s->align);
    if (i < n) {
      unsigned char* ph = out.data + 52 + 32 * i;
      put32(ph, 1);
      put32(ph + 4, s->elfoff);
      put32(ph + 8, s->address);
      put32(ph + 12, s->address);
      put32(ph + 16, s->type == 8 ? 0 : s->size);
      put32(ph + 20, s->size);
      put32(ph + 24, 4 | ((s->flags & 1) ? 2 : 0) | ((s->flags & 4) ? 1 : 0));
      put32(ph + 28, s->align);
      if (s->type != 8)
        memcpy(out.data + s->elfoff, in.data + start + s->offset, s->size);
    }
  }
  {
    unsigned char* sh = out.data + shoff + 40 * (l.count + 1);
    put32(sh, (uint32_t)j);
    put32(sh + 4, 3);
    put32(sh + 16, stroff);
    put32(sh + 20, strsize);
    put32(sh + 32, 1);
  }
  memcpy(out.data + stroff, names, strsize);
  writefile(output, out);
  free(out.data);
  free(in.data);
}

static const unsigned char* section(Buffer elf, const char* name) {
  uint32_t off, str, len;
  unsigned count, entsize, index, i;
  if (elf.size < 52 || memcmp(elf.data, "\177ELF\1\2\1", 7) || be16(elf.data + 16) != 2 ||
      be16(elf.data + 18) != 20)
    fail("expected executable big-endian ELF32 PowerPC");
  off = be32(elf.data + 32);
  count = be16(elf.data + 48);
  entsize = be16(elf.data + 46);
  index = be16(elf.data + 50);
  if (entsize != 40 || !count || index >= count)
    fail("invalid ELF sections");
  range(off, (size_t)count * 40, elf.size);
  str = be32(elf.data + off + 40 * index + 16);
  len = be32(elf.data + off + 40 * index + 20);
  range(str, len, elf.size);
  for (i = 1; i < count; i++) {
    const unsigned char* s = elf.data + off + 40 * i;
    uint32_t no = be32(s);
    if (no >= len || !memchr(elf.data + str + no, 0, len - no))
      fail("invalid ELF section name");
    if (!strcmp((char*)elf.data + str + no, name))
      return s;
  }
  return NULL;
}

static Layout elf_layout(Buffer elf) {
  Layout l = {0};
  uint32_t offset, stringOffset, base = UINT32_MAX;
  unsigned count, i;
  section(elf, "");
  offset = be32(elf.data + 32);
  count = be16(elf.data + 48);
  stringOffset = be32(elf.data + offset + 40 * be16(elf.data + 50) + 16);
  l.entry = be32(elf.data + 24);
  for (i = 1; i < count; ++i) {
    const unsigned char* sh = elf.data + offset + 40 * i;
    uint32_t address = be32(sh + 12);
    if ((be32(sh + 8) & 2) && be32(sh + 20) && address < base)
      base = address;
  }
  if (base == UINT32_MAX)
    fail("ELF has no allocated sections");
  for (i = 1; i < count; ++i) {
    const unsigned char* sh = elf.data + offset + 40 * i;
    uint32_t address = be32(sh + 12), size = be32(sh + 20);
    if (!(be32(sh + 8) & 2) || !size)
      continue;
    if (be32(sh + 4) != 1 && be32(sh + 4) != 8)
      fail("unsupported allocated section type");
    add_section(&l, (const char*)elf.data + stringOffset + be32(sh), address - base, address, size,
                be32(sh + 4), be32(sh + 8), be32(sh + 32));
    l.sections[l.count - 1].elfoff = be32(sh + 16);
  }
  return l;
}

static void packpart(Buffer elf, Layout l, unsigned char* out, uint32_t size) {
  size_t i;
  for (i = 0; i < l.count; i++) {
    Section* p = &l.sections[i];
    if (p->type != 8) {
      range(p->offset, p->size, size);
      range(p->elfoff, p->size, elf.size);
      memcpy(out + p->offset, elf.data + p->elfoff, p->size);
    }
  }
}

static void packdol(Buffer elf, unsigned char* output, uint32_t size) {
  Layout l = elf_layout(elf);
  uint32_t pos = 256, bssStart = UINT32_MAX, bssEnd = 0;
  unsigned text = 0, data = 0;
  size_t i;
  range(0, 256, size);
  qsort(l.sections, l.count, sizeof(l.sections[0]), compare_sections);
  for (i = 0; i < l.count; ++i) {
    Section* s = &l.sections[i];
    uint32_t length;
    unsigned index;
    if (s->type == 8) {
      if (s->address < bssStart)
        bssStart = s->address;
      if (s->address + s->size > bssEnd)
        bssEnd = s->address + s->size;
      continue;
    }
    if (s->flags & 4) {
      if (text >= 7)
        fail("too many DOL text sections");
      index = text++;
    } else {
      if (data >= 11)
        fail("too many DOL data sections");
      index = 7 + data++;
    }
    length = alignup(s->size, 32);
    range(pos, length, size);
    range(s->elfoff, s->size, elf.size);
    memcpy(output + pos, elf.data + s->elfoff, s->size);
    put32(output + index * 4, pos);
    put32(output + 0x48 + index * 4, s->address);
    put32(output + 0x90 + index * 4, length);
    pos += length;
  }
  if (pos != size)
    fail("linked DOL size differs from the apploader header");
  if (bssEnd) {
    put32(output + 0xd8, bssStart);
    put32(output + 0xdc, bssEnd - bssStart);
  }
  put32(output + 0xe0, l.entry);
}

static void pack(const char* original, const char* loader, const char* trailer,
                 const char* output) {
  Buffer meta = readfile(original), a = readfile(loader), b = {0}, out;
  Layout la = elf_layout(a);
  uint32_t body, tsize;
  size_t tail;
  header(meta);
  body = be32(meta.data + 20);
  tsize = be32(meta.data + 24);
  tail = meta.size - 32 - body - tsize;
  out.size = meta.size;
  out.data = calloc(out.size, 1);
  if (!out.data)
    fail("out of memory");
  /* Only the container header and bytes after both components are preserved.
   * Every byte inside either component comes from the linked ELF or padding. */
  memcpy(out.data, meta.data, 32);
  if (la.entry != be32(out.data + 16))
    fail("loader entry point differs from image header");
  packpart(a, la, out.data + 32, body);
  if (tsize) {
    Buffer reference = {meta.data + 32 + body, tsize};
    b = readfile(trailer);
    if (is_dol(reference))
      packdol(b, out.data + 32 + body, tsize);
    else
      packpart(b, elf_layout(b), out.data + 32 + body, tsize);
  }
  if (tail)
    memcpy(out.data + 32 + body + tsize, meta.data + 32 + body + tsize, tail);
  writefile(output, out);
  free(meta.data);
  free(a.data);
  free(b.data);
  free(out.data);
}
/* DTK 1.8 requires a DOL-like input. ALF preserves explicit BSS ranges even
 * when a loader has no SDK ROM-copy/BSS-initialization tables. */
static void putle32(unsigned char* p, uint32_t n) {
  p[0] = (unsigned char)n;
  p[1] = (unsigned char)(n >> 8);
  p[2] = (unsigned char)(n >> 16);
  p[3] = (unsigned char)(n >> 24);
}

static void toalf(const char* input, const char* output) {
  Buffer in = readfile(input), out;
  Layout l = elf_layout(in);
  size_t i, pos = 16;
  out.size = 24 + l.count * 12;
  for (i = 0; i < l.count; ++i) {
    if (l.sections[i].type != 8)
      out.size += l.sections[i].size;
  }
  out.data = calloc(out.size, 1);
  if (!out.data)
    fail("out of memory");
  memcpy(out.data, "RBOF", 4);
  putle32(out.data + 4, 104);
  putle32(out.data + 8, be32(in.data + 24));
  putle32(out.data + 12, (uint32_t)l.count);
  for (i = 0; i < l.count; ++i) {
    Section* p = &l.sections[i];
    uint32_t size = p->type == 8 ? 0 : p->size;
    putle32(out.data + pos, p->address);
    putle32(out.data + pos + 4, size);
    putle32(out.data + pos + 8, p->size);
    pos += 12;
    if (size) {
      range(p->elfoff, size, in.size);
      memcpy(out.data + pos, in.data + p->elfoff, size);
      pos += size;
    }
  }
  putle32(out.data + pos, 4); /* Empty symbol table: count field only. */
  writefile(output, out);
  free(in.data);
  free(out.data);
}

int main(int argc, char** argv) {
  if (argc == 3 && !strcmp(argv[1], "info")) {
    Buffer b = readfile(argv[2]);
    header(b);
    printf("date: %.16s\nentry: 0x%08x\nloader: 0x%x\ntrailer: 0x%x\nextra: %zu\n", b.data,
           be32(b.data + 16), be32(b.data + 20), be32(b.data + 24),
           b.size - 32 - be32(b.data + 20) - be32(b.data + 24));
    free(b.data);
  } else if (argc == 5 && !strcmp(argv[1], "to-elf"))
    toelf(argv[2], argv[3], argv[4]);
  else if (argc == 4 && !strcmp(argv[1], "to-alf"))
    toalf(argv[2], argv[3]);
  else if (argc == 6 && !strcmp(argv[1], "pack"))
    pack(argv[2], argv[3], argv[4], argv[5]);
  else {
    fprintf(stderr,
            "Usage:\n  apploader info IMAGE\n  apploader to-elf IMAGE loader|trailer OUTPUT.elf\n  "
            "apploader to-alf INPUT.elf OUTPUT.alf\n  apploader "
            "pack ORIGINAL.img LOADER.elf TRAILER.elf OUTPUT.img\n");
    return 2;
  }
  return 0;
}
