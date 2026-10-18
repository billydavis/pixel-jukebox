#pragma once

// Picks out beats from a level that is already scaled to the room (0-1), for
// the visualizers. The raw level is no good for that: with the automatic
// scaling, steady music sits near the top all the time, so anything driven by
// it looks the same whether a drum hits or not. This compares the level with
// its own recent average instead, so a kick drum stands out even in loud music.
//
// Call update() once per audio frame (about 32 ms).

class BeatFollower {
public:
  // `punch` is 0 with nothing new happening and up to 1 on a strong beat: how
  // far the level jumped above its recent average. It rises at once and fades
  // over about a quarter of a second, so a beat is visible for a few frames.
  float update(float level);

  float punch() const { return punch_; }
  // The recent average (about a second), 0-1.
  float average() const { return average_; }

  void reset();

private:
  float average_ = 0;
  float punch_ = 0;
};
