// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

//DL facts:
//	Ikaruga uses (nearly) NO display lists!
//  Zelda WW uses TONS of display lists
//  Zelda TP uses almost 100% display lists except menus (we like this!)
//  Super Mario Galaxy has nearly all geometry and more than half of the state in DLs (great!)

// Note that it IS NOT GENERALLY POSSIBLE to precompile display lists! You can compile them as they are
// while interpreting them, and hope that the vertex format doesn't change, though, if you do it right
// when they are called. The reason is that the vertex format affects the sizes of the vertices.
#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/FifoPlayer/FifoRecorder.h"
#include "Core/HW/Memmap.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

bool g_bRecordFifoData = false;
static bool s_bFifoErrorSeen = false;

template <int count>
void ReadU32xn(u32 *bufx16)
{
	g_VideoData.ReadU32xN<count>(bufx16);
}

static u32 InterpretDisplayList(u32 address, u32 size)
{
	u8* old_pVideoData = g_VideoData.GetReadPosition();
	u8* old_pVideoDataEnd = g_VideoData.GetEnd();
	u8* startAddress;

	if (g_use_deterministic_gpu_thread)
		startAddress = (u8*)PopFifoAuxBuffer(size);
	else
		startAddress = (u8*)Memory::GetPointer(address);

	u32 cycles = 0;

	// Avoid the crash if Memory::GetPointer failed ..
	if (startAddress != nullptr)
	{
		g_VideoData.SetReadPosition(startAddress, startAddress + size);

		// temporarily swap dl and non-dl (small "hack" for the stats)
		Statistics::SwapDL();
		OpcodeDecoder_Run(&cycles, true);
		INCSTAT(stats.thisFrame.numDListsCalled);
		// un-swap
		Statistics::SwapDL();
		// reset to the old pointer
		g_VideoData.SetReadPosition(old_pVideoData, old_pVideoDataEnd);
	}
	return cycles;
}

static void InterpretDisplayListPreprocess(u32 address, u32 size)
{
	u8* old_pVideoData = g_VideoData.GetReadPosition();
	u8* old_pVideoDataEnd = g_VideoData.GetEnd();
	u8* startAddress = Memory::GetPointer(address);

	PushFifoAuxBuffer(startAddress, size);

	if (startAddress != nullptr)
	{
		g_VideoData.SetReadPosition(startAddress, startAddress + size);
		OpcodeDecoder_Run<true>(nullptr, true);
		// reset to the old pointer
		g_VideoData.SetReadPosition(old_pVideoData, old_pVideoDataEnd);
	}
}

static void UnknownOpcode(u8 cmd_byte, const void *buffer, bool preprocess)
{
	// TODO(Omega): Maybe dump FIFO to file on this error
	PanicAlert(
		"GFX FIFO: Unknown Opcode (0x%02x @ %p, preprocessing=%s).\n"
		"This means one of the following:\n"
		"* The emulated GPU got desynced, disabling dual core can help\n"
		"* Command stream corrupted by some spurious memory bug\n"
		"* This really is an unknown opcode (unlikely)\n"
		"* Some other sort of bug\n\n"
		"Further errors will be sent to the Video Backend log and\n"
		"Dolphin will now likely crash or hang. Enjoy.",
		cmd_byte,
		buffer,
		preprocess ? "yes" : "no");

	{
		SCPFifoStruct &fifo = CommandProcessor::fifo;

		PanicAlert(
			"Illegal command %02x\n"
			"CPBase: 0x%08x\n"
			"CPEnd: 0x%08x\n"
			"CPHiWatermark: 0x%08x\n"
			"CPLoWatermark: 0x%08x\n"
			"CPReadWriteDistance: 0x%08x\n"
			"CPWritePointer: 0x%08x\n"
			"CPReadPointer: 0x%08x\n"
			"CPBreakpoint: 0x%08x\n"
			"bFF_GPReadEnable: %s\n"
			"bFF_BPEnable: %s\n"
			"bFF_BPInt: %s\n"
			"bFF_Breakpoint: %s\n"
			"bFF_GPLinkEnable: %s\n"
			"bFF_HiWatermarkInt: %s\n"
			"bFF_LoWatermarkInt: %s\n"
			, cmd_byte, fifo.CPBase, fifo.CPEnd, fifo.CPHiWatermark, fifo.CPLoWatermark, fifo.CPReadWriteDistance
			, fifo.CPWritePointer, fifo.CPReadPointer, fifo.CPBreakpoint
			, fifo.bFF_GPReadEnable ? "true" : "false"
			, fifo.bFF_BPEnable ? "true" : "false"
			, fifo.bFF_BPInt ? "true" : "false"
			, fifo.bFF_Breakpoint ? "true" : "false"
			, fifo.bFF_GPLinkEnable ? "true" : "false"
			, fifo.bFF_HiWatermarkInt ? "true" : "false"
			, fifo.bFF_LoWatermarkInt ? "true" : "false"
			);
	}
}

template<bool sizeCheck, bool is_preprocess>
__forceinline u32 Decode(bool in_display_list)
{
	

	return cycles;
}


DataReadU32xNfunc DataReadU32xFuncs[16] = {
	ReadU32xn<1>,
	ReadU32xn<2>,
	ReadU32xn<3>,
	ReadU32xn<4>,
	ReadU32xn<5>,
	ReadU32xn<6>,
	ReadU32xn<7>,
	ReadU32xn<8>,
	ReadU32xn<9>,
	ReadU32xn<10>,
	ReadU32xn<11>,
	ReadU32xn<12>,
	ReadU32xn<13>,
	ReadU32xn<14>,
	ReadU32xn<15>,
	ReadU32xn<16>
};

void OpcodeDecoder_Init()
{
	s_bFifoErrorSeen = false;
}


void OpcodeDecoder_Shutdown()
{
}

template <bool is_preprocess>
u8* OpcodeDecoder_Run(u32* cycles, bool in_display_list)
{
	u32 totalCycles = 0;
	u8* opcodeStart;
	while (true)
	{
		opcodeStart = g_VideoData.GetReadPosition();
		if (!g_VideoData.size())
			goto end;

		u8 cmd_byte = g_VideoData.Read<u8>();
		size_t distance = g_VideoData.size();

		switch (cmd_byte)
		{
		case GX_NOP:
		{
			totalCycles += GX_NOP_CYCLES; // Hm, this means that we scan over nop streams pretty slowly...
		}
		break;
		case GX_UNKNOWN_RESET:
		{
			totalCycles += GX_NOP_CYCLES; // Datel software uses this command
			DEBUG_LOG(VIDEO, "GX Reset?: %08x", cmd_byte);
		}
		break;
		case GX_LOAD_CP_REG:
		{
			if (distance < GX_LOAD_CP_REG_SIZE)
				goto end;
			totalCycles += GX_LOAD_CP_REG_CYCLES;
			u8 sub_cmd = g_VideoData.Read<u8>();
			u32 value = g_VideoData.Read<u32>();
			LoadCPReg(sub_cmd, value, is_preprocess);
			if (!is_preprocess)
				INCSTAT(stats.thisFrame.numCPLoads);
		}
		break;
		case GX_LOAD_XF_REG:
		{
			if (distance < GX_LOAD_XF_REG_SIZE)
				goto end;
			u32 Cmd2 = g_VideoData.Read<u32>();
			distance -= GX_LOAD_XF_REG_SIZE;
			int transfer_size = ((Cmd2 >> 16) & 15) + 1;
			if (distance < (transfer_size * sizeof(u32)))
				goto end;
			totalCycles += GX_LOAD_XF_REG_BASE_CYCLES + GX_LOAD_XF_REG_TRANSFER_CYCLES * transfer_size;
			if (!is_preprocess)
			{
				u32 xf_address = Cmd2 & 0xFFFF;
				LoadXFReg(transfer_size, xf_address);
				INCSTAT(stats.thisFrame.numXFLoads);
			}
		}
		break;
		case GX_LOAD_INDX_A: //used for position matrices
		{
			if (distance < GX_LOAD_INDX_A_SIZE)
				goto end;
			totalCycles += GX_LOAD_INDX_A_CYCLES;
			if (is_preprocess)
				PreprocessIndexedXF(g_VideoData.Read<u32>(), 0xC);
			else
				LoadIndexedXF(g_VideoData.Read<u32>(), 0xC);
		}
		break;
		case GX_LOAD_INDX_B: //used for normal matrices
		{
			if (distance < GX_LOAD_INDX_B_SIZE)
				goto end;
			totalCycles += GX_LOAD_INDX_B_CYCLES;
			if (is_preprocess)
				PreprocessIndexedXF(g_VideoData.Read<u32>(), 0xD);
			else
				LoadIndexedXF(g_VideoData.Read<u32>(), 0xD);
		}
		break;
		case GX_LOAD_INDX_C: //used for postmatrices
		{
			if (distance < GX_LOAD_INDX_C_SIZE)
				goto end;
			totalCycles += GX_LOAD_INDX_C_CYCLES;
			if (is_preprocess)
				PreprocessIndexedXF(g_VideoData.Read<u32>(), 0xE);
			else
				LoadIndexedXF(g_VideoData.Read<u32>(), 0xE);
		}
		break;
		case GX_LOAD_INDX_D: //used for lights
		{
			if (distance < GX_LOAD_INDX_D_SIZE)
				goto end;
			totalCycles += GX_LOAD_INDX_D_CYCLES;
			if (is_preprocess)
				PreprocessIndexedXF(g_VideoData.Read<u32>(), 0xF);
			else
				LoadIndexedXF(g_VideoData.Read<u32>(), 0xF);
		}
		break;
		case GX_CMD_CALL_DL:
		{
			if (distance < GX_CMD_CALL_DL_SIZE)
				goto end;
			u32 address = g_VideoData.Read<u32>();
			u32 count = g_VideoData.Read<u32>();
			if (in_display_list)
			{
				totalCycles += GX_CMD_CALL_DL_BASE_CYCLES;
				WARN_LOG(VIDEO, "recursive display list detected");
			}
			else
			{
				if (is_preprocess)
					InterpretDisplayListPreprocess(address, count);
				else
					totalCycles += GX_CMD_CALL_DL_BASE_CYCLES + InterpretDisplayList(address, count);
			}
		}
		break;
		case GX_CMD_UNKNOWN_METRICS: // zelda 4 swords calls it and checks the metrics registers after that
		{
			totalCycles += GX_CMD_UNKNOWN_METRICS_CYCLES;
			DEBUG_LOG(VIDEO, "GX 0x44: %08x", cmd_byte);
		}
		break;
		case GX_CMD_INVL_VC: // Invalidate Vertex Cache	
		{
			totalCycles += GX_CMD_INVL_VC_CYCLES;
			DEBUG_LOG(VIDEO, "Invalidate (vertex cache?)");
		}
		break;
		case GX_LOAD_BP_REG:
		{
			if (distance < GX_LOAD_BP_REG_SIZE)
				goto end;
			totalCycles += GX_LOAD_BP_REG_CYCLES;
			u32 bp_cmd = g_VideoData.Read<u32>();
			if (is_preprocess)
			{
				LoadBPRegPreprocess(bp_cmd);
			}
			else
			{
				LoadBPReg(bp_cmd);
				INCSTAT(stats.thisFrame.numBPLoads);
			}
		}
		break;
		// draw primitives 
		default:
			if ((cmd_byte & GX_DRAW_PRIMITIVES) == 0x80)
			{
				// load vertices
				if (distance < GX_DRAW_PRIMITIVES_SIZE)
					goto end;

				u32 count = g_VideoData.Read<u16>();
				distance -= GX_DRAW_PRIMITIVES_SIZE;
				if (count)
				{
					VertexLoaderParameters parameters;
					parameters.count = count;
					parameters.buf_size = distance;
					parameters.primitive = (cmd_byte & GX_PRIMITIVE_MASK) >> GX_PRIMITIVE_SHIFT;
					parameters.vtx_attr_group = cmd_byte & GX_VAT_MASK;
					parameters.needloaderrefresh = (g_main_cp_state.attr_dirty & (1 << parameters.vtx_attr_group)) != 0;
					parameters.skip_draw = g_bSkipCurrentFrame;
					parameters.VtxDesc = &g_main_cp_state.vtx_desc;
					parameters.VtxAttr = &g_main_cp_state.vtx_attr[parameters.vtx_attr_group];
					parameters.source = g_VideoData.GetReadPosition();
					g_main_cp_state.attr_dirty &= ~(1 << parameters.vtx_attr_group);
					u32 readsize = 0;
					u32 writesize = 0;
					if (VertexLoaderManager::ConvertVertices(parameters, readsize, writesize))
					{
						totalCycles += GX_NOP_CYCLES + GX_DRAW_PRIMITIVES_CYCLES * parameters.count;
						g_VideoData.ReadSkip(readsize);
						VertexManagerBase::s_pCurBufferPointer += writesize;
					}
					else
					{
						goto end;
					}
				}
				else
				{
					totalCycles += GX_NOP_CYCLES;
				}
			}
			else
			{
				if (!s_bFifoErrorSeen)
					UnknownOpcode(cmd_byte, opcodeStart, is_preprocess);
				ERROR_LOG(VIDEO, "FIFO: Unknown Opcode(0x%02x @ %p, preprocessing = %s)", cmd_byte, opcodeStart, is_preprocess ? "yes" : "no");
				s_bFifoErrorSeen = true;
				totalCycles += 1;
			}
			break;
		}

		// Display lists get added directly into the FIFO stream
		if (!is_preprocess && g_bRecordFifoData && cmd_byte != GX_CMD_CALL_DL)
		{
			const u8* opcodeEnd;
			opcodeEnd = g_VideoData.GetReadPosition();
			FifoRecorder::GetInstance().WriteGPCommand(opcodeStart, u32(opcodeEnd - opcodeStart));
		}
	}
end:
	if (cycles)
	{
		*cycles = totalCycles;
	}
	return opcodeStart;
}

template u8* OpcodeDecoder_Run<true>(u32* cycles, bool in_display_list);
template u8* OpcodeDecoder_Run<false>(u32* cycles, bool in_display_list);
