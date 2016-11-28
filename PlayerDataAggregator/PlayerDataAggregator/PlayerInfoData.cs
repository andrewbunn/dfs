using Newtonsoft.Json;
using System.Collections;
using System.Collections.Generic;

namespace PlayerDataAggregator
{
    public class PlayerInfoData
    {
        [JsonProperty("players")]
        public PlayerList PlayerList { get; set; }
    }

    public class PlayerList
    {
        [JsonProperty("player")]
        public IEnumerable<PlayerInfo> Players { get; set; }
    }

    public class PlayerInfo
    {
        [JsonProperty("id")]
        public int Id { get; set; }

        [JsonProperty("name")]
        public string Name { get; set; }

        [JsonProperty("position")]
        public string Position { get; set; }

        [JsonProperty("team")]
        public string Team { get; set; }
    }
}