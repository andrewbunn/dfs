using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace GeneratePlayerData
{
    public class EmpeopledBlogPostResponse
    {
        [JsonProperty("data")]
        public EmpeopledData Data { get; set; }
    }

    public class EmpeopledData
    {
        [JsonProperty("content")]
        public EmpeopledContent Content { get; set; }
    }

    public class EmpeopledContent
    {
        [JsonProperty("body")]
        public string Body { get; set; }
    }
}
