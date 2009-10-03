// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "PPCCache.h"
#include "../HW/Memmap.h"
#include "PowerPC.h"

namespace PowerPC
{

	u32 plru_mask[8] = {11,11,19,19,37,37,69,69};
	u32 plru_value[8] = {11,3,17,1,36,4,64,0};

	InstructionCache::InstructionCache()
	{
		for (u32 m = 0; m < 0xff; m++)
		{
			u32 w = 0;
			while (m & (1<<w)) w++;
			way_from_valid[m] = w;
		}

		for (u32 m = 0; m < 128; m++)
		{
			u32 b[7];
			for (int i = 0; i < 7; i++) b[i] = m & (1<<i);
			u32 w;
			if (b[0])
				if (b[2])
					if (b[6])
						w = 7;
					else
						w = 6;
				else
					if (b[5])
						w = 5;
					else
						w = 4;
			else
				if (b[1])
					if (b[4])
						w = 3;
					else
						w = 2;
				else
					if (b[3])
						w = 1;
					else
						w = 0;
			way_from_plru[m] = w;
		}
	}

	void InstructionCache::Reset()
	{
		memset(valid, 0, sizeof(valid));
		memset(plru, 0, sizeof(plru));
#ifdef FAST_ICACHE
		memset(lookup_table, 0xff, sizeof(lookup_table));
#endif
	}

	void InstructionCache::Invalidate(u32 addr)
	{
		if (!HID0.ICE)
			return;
		// invalidates the whole set
		u32 set = (addr >> 5) & 0x7f;
#ifdef FAST_ICACHE
		for (int i = 0; i < 8; i++)
			if (valid[set] & (1<<i))
			{
				lookup_table[((tags[set][i] << 7) | set) & 0xfffff] = 0xff;
			}
#endif
		valid[set] = 0;
	}

	u32 InstructionCache::ReadInstruction(u32 addr)
	{
		if (!HID0.ICE) // instuction cache is disabled
			return Memory::ReadUnchecked_U32(addr);
		u32 set = (addr >> 5) & 0x7f;
		u32 tag = addr >> 12;
#ifdef FAST_ICACHE
		u32 t = lookup_table[(addr>>5) & 0xfffff];		
#else
		u32 t = 0xff;
		for (u32 i = 0; i < 8; i++)
			if (tags[set][i] == tag && (valid[set] & (1<<i)))
			{
				t = i;
				break;
			}
#endif
		if (t == 0xff) // load to the cache
		{
			if (HID0.ILOCK) // instruction cache is locked
				return Memory::ReadUnchecked_U32(addr);
			// select a way
			if (valid[set] != 0xff)
				t = way_from_valid[valid[set]];
			else
				t = way_from_plru[plru[set]];
			// load
			u8 *p = Memory::GetPointer(addr & ~0x1f);
			memcpy(data[set][t], p, 32);
#ifdef FAST_ICACHE
			if (valid[set] & (1<<t))
				lookup_table[((tags[set][t] << 7) | set) & 0xfffff] = 0xff;
			lookup_table[(addr>>5) & 0xfffff] = t;
#endif
			tags[set][t] = tag;
			valid[set] |= 1<<t;
		}
		// update plru
		plru[set] = (plru[set] & ~plru_mask[t]) | plru_value[t];
		return Common::swap32(data[set][t][(addr>>2)&7]);
	}

}