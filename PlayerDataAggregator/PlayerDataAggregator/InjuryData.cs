using Newtonsoft.Json;
using System.Collections.Generic;

namespace PlayerDataAggregator
{
    public class InjuryData
    {
        [JsonProperty("injuries")]
        public InjuriesList InjuriesList { get; set; }
    }

    public class InjuriesList
    {
        [JsonProperty("injury")]
        public IEnumerable<Injury> Injuries { get; set; }

        [JsonProperty("week")]
        public int Week { get; set; }
    }

    public class Injury
    {
        [JsonProperty("id")]
        public int PlayerId { get; set; }

        [JsonProperty("status")]
        public string Status { get; set; }

        [JsonProperty("details")]
        public string Details { get; set; }
    }
}