using Newtonsoft.Json;
using Newtonsoft.Json.Converters;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace GeneratePlayerData
{
    public class YahooResponse
    {
        [JsonIgnore]
        public DateTime AsOfDateTime { get; set; }

        [JsonProperty("players")]
        public YahooResult Result { get; set; }
    }

    public class YahooResult
    {
        [JsonProperty("result")]
        public IEnumerable<YahooPlayerData> Players { get; set; }
    }
}
