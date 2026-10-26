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

static Layout loader_layout(Buffer image) {
  Layout l = {0};
  const unsigned char* body = image.data + 32;
  uint32_t size = be32(image.data + 20), data = size, bss = size, zero = size;
  uint32_t i, address;
  l.entry = be32(image.data + 16);
  if (l.entry < LOADER_ADDRESS || (uint64_t)l.entry >= (uint64_t)LOADER_ADDRESS + size)
    fail("loader entry is outside the boot ROM load range");
  while (zero && body[zero - 1] == 0)
    --zero;

  /* Literal addresses used by the code identify the pooled strings and static
   * storage. Do not classify arbitrary printable instruction bytes as data. */
  for (i = 0; i + 8 <= size; i += 4) {
    uint32_t offset;
    if (!address_pair(be32(body + i), be32(body + i + 4), &address) || address < LOADER_ADDRESS)
      continue;
    offset = address - LOADER_ADDRESS;
    if (offset >= size)
      continue;
    if (offset > i && is_string(body, size, offset) && offset < data)
      data = offset;
    if (offset >= zero && !(offset & 31) && offset < bss)
      bss = offset;
  }
  if (data == size || bss == size || data >= bss || (data & 31))
    fail("cannot establish loader code/data/BSS boundaries from its references");
  if (l.entry >= LOADER_ADDRESS + data)
    fail("loader entry overlaps detected data");
  add_section(&l, ".text", 0, LOADER_ADDRESS, data, 1, 6, 32);
  add_section(&l, ".data", data, LOADER_ADDRESS + data, bss - data, 1, 3, 32);
  add_section(&l, ".bss", bss, LOADER_ADDRESS + bss, size - bss, 8, 3, 32);
  return l;
}

static Layout trailer_layout(Buffer image) {
  Layout l = {0};
  const unsigned char* body = image.data + 32 + be32(image.data + 20);
  uint32_t size = be32(image.data + 24), table, base = 0, count = 0, pos, i;
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
    if (count >= 2 && pos + 12 <= size && be32(body + table + 8) >= pos + 12)
      break;
  }
  if (table + 36 > size || count < 2)
    fail("cannot find a complete trailer ROM-copy table");
  l.entry = base;
  for (i = 0; i < count; ++i) {
    uint32_t address = be32(body + table + i * 12);
    uint32_t length = be32(body + table + i * 12 + 8);
    char fallback[32];
    const char* name;
    if (count == sizeof(names) / sizeof(names[0]))
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
    if (i >= 2 || !length || address < base || address - base > size ||
        length > size - (address - base))
      fail("invalid trailer BSS initialization table");
    add_section(&l, name, address - base, address, length, 8, 3, 32);
  }
  fail("unterminated trailer BSS initialization table");
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
  for (i = 0; i < n; i++) {
    Section* s = &l.sections[i];
    range(s->offset, s->size, size);
    phcount++;
    for (j = 0; j < s->size; j++) {
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
  /* Non-allocating metadata is excluded by elf2dol; pack uses the original header. */
  strcpy(l.sections[l.count].name, ".apploader.header");
  l.sections[l.count].size = 32;
  l.sections[l.count].type = 1;
  l.sections[l.count++].align = 1;
  if (!l.trailer && in.size > 32 + (size_t)be32(in.data + 20) + be32(in.data + 24)) {
    Section* s = &l.sections[l.count++];
    strcpy(s->name, ".apploader.tail");
    s->size = (uint32_t)(in.size - 32 - be32(in.data + 20) - be32(in.data + 24));
    s->type = 1;
    s->align = 1;
  }
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
    } else if (!strcmp(s->name, ".apploader.header"))
      memcpy(out.data + s->elfoff, in.data, 32);
    else
      memcpy(out.data + s->elfoff, in.data + in.size - s->size, s->size);
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
  }
  return l;
}

static void packpart(Buffer elf, Layout l, unsigned char* out, uint32_t size) {
  size_t i;
  for (i = 0; i < l.count; i++) {
    Section* p = &l.sections[i];
    const unsigned char* s = section(elf, p->name);
    uint32_t off, sz;
    if (!s)
      fail("missing linked section");
    off = be32(s + 16);
    sz = be32(s + 20);
    if (be32(s + 12) != p->address || sz != p->size) {
      fprintf(stderr, "section %s: address %08x/%08x size %x/%x\n", p->name, be32(s + 12),
              p->address, sz, p->size);
      fail("linked layout differs");
    }
    range(p->offset, sz, size);
    if (be32(s + 4) == 8) {
      if (p->type != 8)
        fail("unexpected bss section");
    } else {
      range(off, sz, elf.size);
      memcpy(out + p->offset, elf.data + off, sz);
    }
  }
}

static void pack(const char* metadata, const char* loader, const char* trailer,
                 const char* output) {
  Buffer meta = readfile(metadata), a = readfile(loader), b = {0}, out;
  Layout la = elf_layout(a);
  const unsigned char *h = section(meta, ".apploader.header"),
                      *t = section(meta, ".apploader.tail");
  uint32_t body, tsize, tail = 0, hoff;
  if (!h || be32(h + 20) != 32)
    fail("invalid loader metadata/layout");
  hoff = be32(h + 16);
  range(hoff, 32, meta.size);
  body = be32(meta.data + hoff + 20);
  tsize = be32(meta.data + hoff + 24);
  if (t) {
    tail = be32(t + 20);
    range(be32(t + 16), tail, meta.size);
  }
  if ((uint64_t)32 + body + tsize + tail > MAX_FILE)
    fail("image too large");
  out.size = 32 + (size_t)body + tsize + tail;
  out.data = calloc(out.size, 1);
  if (!out.data)
    fail("out of memory");
  memcpy(out.data, meta.data + hoff, 32);
  section(a, "");
  if (be32(a.data + 24) != be32(out.data + 16))
    fail("loader entry point differs from image header");
  packpart(a, la, out.data + 32, body);
  if (tsize) {
    Layout lb;
    b = readfile(trailer);
    lb = elf_layout(b);
    packpart(b, lb, out.data + 32 + body, tsize);
  }
  if (tail)
    memcpy(out.data + 32 + body + tsize, meta.data + be32(t + 16), tail);
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
  section(in, ""); /* Validate the ELF before reading its header. */
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
    const unsigned char* sh = section(in, p->name);
    uint32_t size = p->type == 8 ? 0 : p->size;
    if (!sh || be32(sh + 12) != p->address || be32(sh + 20) != p->size)
      fail("ELF does not match layout");
    putle32(out.data + pos, p->address);
    putle32(out.data + pos + 4, size);
    putle32(out.data + pos + 8, p->size);
    pos += 12;
    if (size) {
      range(be32(sh + 16), size, in.size);
      memcpy(out.data + pos, in.data + be32(sh + 16), size);
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
            "pack METADATA.elf LOADER.elf TRAILER.elf OUTPUT.img\n");
    return 2;
  }
  return 0;
}
