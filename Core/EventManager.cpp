#include "stdafx.h"
#include "EventManager.h"
#include "DebuggerTypes.h"
#include "CPU.h"
#include "PPU.h"
#include "Debugger.h"
#include "DebugBreakHelper.h"
#include "DefaultVideoFilter.h"

EventManager::EventManager(Debugger *debugger, CPU *cpu, PPU *ppu, EmulationSettings *settings)
{
	_debugger = debugger;
	_cpu = cpu;
	_ppu = ppu;
	_settings = settings;

	_ppuBuffer = new uint16_t[256*240];
	memset(_ppuBuffer, 0, 256*240 * sizeof(uint16_t));
}

EventManager::~EventManager()
{
	delete[] _ppuBuffer;
}

void EventManager::AddSpecialEvent(DebugEventType type)
{
	if(type == DebugEventType::BgColorChange) {
		AddDebugEvent(DebugEventType::BgColorChange, _ppu->GetCurrentBgColor());
	}
}

void EventManager::AddDebugEvent(DebugEventType type, uint16_t address, uint8_t value, int16_t breakpointId, int8_t ppuLatch)
{
	_debugEvents.push_back({
		(uint16_t)_ppu->GetCurrentCycle(),
		(int16_t)_ppu->GetCurrentScanline(),
		_cpu->GetDebugPC(),
		address,
		breakpointId,
		type,
		value,
		ppuLatch,
	});
}

void EventManager::GetEvents(DebugEventInfo *eventArray, uint32_t &maxEventCount, bool getPreviousFrameData)
{
	DebugBreakHelper breakHelper(_debugger);

	vector<DebugEventInfo> &events = getPreviousFrameData ? _prevDebugEvents : _debugEvents;
	uint32_t eventCount = std::min(maxEventCount, (uint32_t)events.size());
	memcpy(eventArray, events.data(), eventCount * sizeof(DebugEventInfo));
	maxEventCount = eventCount;
}

uint32_t EventManager::GetEventCount(bool getPreviousFrameData)
{
	DebugBreakHelper breakHelper(_debugger);
	return (uint32_t)(getPreviousFrameData ? _prevDebugEvents.size() : _debugEvents.size());
}

void EventManager::ClearFrameEvents()
{
	_prevDebugEvents = _debugEvents;
	_debugEvents.clear();
	AddDebugEvent(DebugEventType::BgColorChange, _ppu->GetCurrentBgColor());
}

void EventManager::DrawEvent(DebugEventInfo &evt, bool drawBackground, uint32_t *buffer, EventViewerDisplayOptions &options)
{
	bool showEvent = false;
	uint32_t color = 0;
	switch(evt.Type) {
		case DebugEventType::None: 
		case DebugEventType::BgColorChange:
			break;

		case DebugEventType::Breakpoint: showEvent = options.ShowMarkedBreakpoints; color = options.BreakpointColor; break;
		case DebugEventType::Irq: showEvent = options.ShowIrq; color = options.IrqColor; break;
		case DebugEventType::Nmi: showEvent = options.ShowNmi; color = options.NmiColor; break;
		case DebugEventType::DmcDmaRead: showEvent = options.ShowDmcDmaReads; color = options.DmcDmaReadColor; break;
		case DebugEventType::SpriteZeroHit: showEvent = options.ShowSpriteZeroHit; color = options.SpriteZeroHitColor; break;

		case DebugEventType::MapperRegisterWrite:
			showEvent = options.ShowMapperRegisterWrites;
			color = options.MapperRegisterWriteColor;
			break;

		case DebugEventType::MapperRegisterRead:
			showEvent = options.ShowMapperRegisterReads;
			color = options.MapperRegisterReadColor;
			break;

		case DebugEventType::ApuRegisterWrite:
			showEvent = options.ShowApuRegisterWrites;
			color = options.ApuRegisterWriteColor;
			break;

		case DebugEventType::ApuRegisterRead:
			showEvent = options.ShowApuRegisterReads;
			color = options.ApuRegisterReadColor;
			break;

		case DebugEventType::ControlRegisterWrite:
			showEvent = options.ShowControlRegisterWrites;
			color = options.ControlRegisterWriteColor;
			break;

		case DebugEventType::ControlRegisterRead:
			showEvent = options.ShowControlRegisterReads;
			color = options.ControlRegisterReadColor;
			break;

		case DebugEventType::PpuRegisterWrite:
			showEvent = options.ShowPpuRegisterWrites[evt.Address & 0x07];
			color = options.PpuRegisterWriteColors[evt.Address & 0x07];
			break;

		case DebugEventType::PpuRegisterRead:
			showEvent = options.ShowPpuRegisterReads[evt.Address & 0x07];
			color = options.PpuRegisterReadColors[evt.Address & 0x07];
			break;
	}

	if(!showEvent) {
		return;
	}

	if(drawBackground) {
		color = 0xFF000000 | ((color >> 1) & 0x7F7F7F);
	} else {
		_sentEvents.push_back(evt);
		color |= 0xFF000000;
	}

	uint32_t y = std::min<uint32_t>((evt.Scanline + 1) * 2, _scanlineCount * 2);
	uint32_t x = evt.Cycle * 2;
	DrawDot(x, y, color, drawBackground, buffer);
}

void EventManager::DrawDot(uint32_t x, uint32_t y, uint32_t color, bool drawBackground, uint32_t* buffer)
{
	int iMin = drawBackground ? -2 : 0;
	int iMax = drawBackground ? 3 : 1;
	int jMin = drawBackground ? -2 : 0;
	int jMax = drawBackground ? 3 : 1;

	for(int i = iMin; i <= iMax; i++) {
		for(int j = jMin; j <= jMax; j++) {
			int32_t pos = (y + i) * 341 * 2 + x + j;
			if(pos < 0 || pos >= 341 * 2 * (int)_scanlineCount * 2) {
				continue;
			}
			buffer[pos] = color;
		}
	}
}

void EventManager::DrawPixel(uint32_t *buffer, int32_t x, uint32_t y, uint32_t color)
{
	if(x < 0) {
		x += 341;
		y--;
	} else if(x >= 341) {
		x -= 341;
		y++;
	}

	buffer[y * 341 * 4 + x * 2] = color;
	buffer[y * 341 * 4 + x * 2 + 1] = color;
	buffer[y * 341 * 4 + 341*2 + x * 2] = color;
	buffer[y * 341 * 4 + 341*2 + x * 2 + 1] = color;
}

void EventManager::DrawNtscBorders(uint32_t *buffer)
{
	//Generate array of bg color for all pixels on the screen
	uint32_t currentPos = 0;
	uint16_t currentColor = 0;
	vector<uint16_t> bgColor;
	bgColor.resize(341 * 243);
	uint32_t* pal = _settings->GetRgbPalette();

	for(DebugEventInfo &evt : _snapshot) {
		if(evt.Type == DebugEventType::BgColorChange) {
			uint32_t pos = ((evt.Scanline + 1) * 341) + evt.Cycle;
			if(pos >= currentPos && evt.Scanline < 242) {
				std::fill(bgColor.begin() + currentPos, bgColor.begin() + pos, currentColor);
				currentColor = evt.Address;
				currentPos = pos;
			}
		}
	}
	std::fill(bgColor.begin() + currentPos, bgColor.end(), currentColor);

	for(uint32_t y = 1; y < 241; y++) {
		//Pulse
		uint32_t basePos = y * 341;
		DrawPixel(buffer, -15, y, pal[bgColor[basePos - 16] & 0x30]);

		//Left border
		for(int32_t x = 0; x < 15; x++) {
			DrawPixel(buffer, -x, y, pal[bgColor[basePos - x]]);
		}

		//Right border
		for(int32_t x = 0; x < 11; x++) {
			DrawPixel(buffer, 257+x, y, pal[bgColor[basePos + 257 + x]]);
		}
	}

	for(uint32_t y = 240; y < 242; y++) {
		//Bottom border
		uint32_t basePos = y * 341;
		DrawPixel(buffer, 326, y, pal[bgColor[basePos + 326] & 0x30]);
		for(int32_t x = 0; x < 282; x++) {
			DrawPixel(buffer, 327 + x, y, pal[bgColor[basePos + 327 + x]]);
		}
	}
}
