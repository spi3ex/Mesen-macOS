#pragma once

#include "stdafx.h"
#include "IMemoryHandler.h"
#include "Snapshotable.h"
#include "EmulationSettings.h"

class Console;
class SquareChannel;
class TriangleChannel;
class NoiseChannel;
class DeltaModulationChannel;
class ApuFrameCounter;
class SoundMixer;
enum class FrameType;
enum class NesModel;

class APU : public Snapshotable, public IMemoryHandler
{
	friend ApuFrameCounter;

	private:
		bool _apuEnabled;
		bool _needToRun;

		uint32_t _previousCycle;
		uint32_t _currentCycle;

		std::unique_ptr<SquareChannel> _squareChannel[2];
		std::unique_ptr<TriangleChannel> _triangleChannel;
		std::unique_ptr<NoiseChannel> _noiseChannel;
		std::unique_ptr<DeltaModulationChannel> _deltaModulationChannel;
		std::unique_ptr<ApuFrameCounter> _frameCounter;

		std::shared_ptr<Console> _console;
		std::shared_ptr<SoundMixer> _mixer;
		EmulationSettings* _settings;

		NesModel _nesModel;

	private:
		__forceinline bool NeedToRun(uint32_t currentCycle);

		void FrameCounterTick(FrameType type);
		uint8_t GetStatus();

	protected:
		void StreamState(bool saving) override;

	public:
		APU(std::shared_ptr<Console> console);
		~APU();

		void Reset(bool softReset);
		void SetNesModel(NesModel model, bool forceInit = false);

		uint8_t ReadRAM(uint16_t addr) override;
		uint8_t PeekRAM(uint16_t addr) override;
		void WriteRAM(uint16_t addr, uint8_t value) override;
		void GetMemoryRanges(MemoryRanges &ranges) override;

		ApuState GetState();

		void Exec();
		void ProcessCpuClock();
		void Run();
		void EndFrame();

		void AddExpansionAudioDelta(AudioChannel channel, int16_t delta);
		void SetApuStatus(bool enabled);
		bool IsApuEnabled();
		uint16_t GetDmcReadAddress();
		void SetDmcReadBuffer(uint8_t value);
		void SetNeedToRun();
};
