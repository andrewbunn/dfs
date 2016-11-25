using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace GeneratePlayerData
{
    public enum PositionEnum
    {
        qb = 0,
        rb = 1,
        wr = 2,
        te = 3,
        def = 4
    };

    public class YahooPlayerData
    {
        [JsonProperty("sport")]
        public string Sport { get; set; }

        [JsonProperty("playerCode")]
        public string PlayerCode { get; set; }

        [JsonProperty("gameCode")]
        public string GameCode { get; set; }

        [JsonProperty("position")]
        public string Position { get; set; }

        [JsonIgnore]
        public PositionEnum PositionEnum
        {
            get
            {
                return (PositionEnum)Enum.Parse(typeof(PositionEnum), this.Position.ToLower());
            }
        }

        [JsonProperty("team")]
        public string Team { get; set; }

        [JsonProperty("salary")]
        public int Salary { get; set; }

        [JsonProperty("fppg")]
        public decimal FantasyPointsPerGame { get; set; }

        [JsonProperty("name")]
        public string Name { get; set; }

        [JsonProperty("homeTeam")]
        public string HomeTeam { get; set; }

        [JsonProperty("awayTeam")]
        public string AwayTeam { get; set; }

        [JsonProperty("gameStartTime")]
        public DateTime GameStartTime { get; set; }
    }
}
