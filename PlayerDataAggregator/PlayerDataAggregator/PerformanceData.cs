using Newtonsoft.Json;
using System.Collections.Generic;

namespace PlayerDataAggregator
{
    public class PerformanceData
    {
        [JsonProperty("playerScores")]
        public PlayerScores PlayerScores { get; set; }
    }

    public class PlayerScores
    {
        [JsonProperty("playerScore")]
        public IEnumerable<PlayerScore> Scores { get; set; }

        [JsonProperty("week")]
        public int Week { get; set; }
    }

    public class PlayerScore
    {
        [JsonProperty("id")]
        public int PlayerId { get; set; }

        [JsonProperty("score")]
        public float Score { get; set; }
    }
}