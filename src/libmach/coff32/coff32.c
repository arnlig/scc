#include <stdio.h>

#include <scc/mach.h>

#include "../libmach.h"
#include "coff32.h"

struct objops coff32 = {
	.probe = coff32probe,
	.new = coff32new,
	.read = coff32read,
	.getidx = coff32getidx,
	.setidx = coff32setidx,
	.addr2line = coff32addr2line,
	.strip = coff32strip,
	.del = coff32del,
	.write = coff32write,
	.getsym = coff32getsym,
	.getsec = coff32getsec,
};
