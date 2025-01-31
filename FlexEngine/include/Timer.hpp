#pragma once

namespace flex
{
	struct Timer
	{
		Timer() = default;

		Timer(sec duration);
		Timer(sec duration, sec initialRemaining);

		// Returns true when timer is complete
		bool Update();
		void Restart();
		void Complete();
		bool IsComplete() const;

		sec duration = 0.0f;
		sec remaining = 0.0f;
		bool bUseUnpausedDeltaTime = true;

	};
} // namespace flex
