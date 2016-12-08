using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace PlayerDataAggregator
{
    public class FFAnalyticsProjectionsResponse
    {
        [JsonProperty("GridData")]
        public List<PlayerProjection> PlayerProjections { get; set; }
    }

    public class PlayerProjection
    {
        [JsonProperty("playerId")]
        public int PlayerId { get; set; }

        [JsonProperty("playername")]
        public string PlayerName { get; set; }

        [JsonProperty("playerposition")]
        public string PlayerPositon { get; set; }

        [JsonProperty("points")]
        public float Points { get; set; }
    }
}
