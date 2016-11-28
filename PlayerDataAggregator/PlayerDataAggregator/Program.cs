using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace PlayerDataAggregator
{
    class Program
    {
        private const int LEAGUE_ID = 11289;
        // Year, ReportType, LeagueID, Week
        private const string BASE_ADDRESS = "http://www70.myfantasyleague.com/{0}/export?TYPE={1}&L={2}&W={3}&JSON=1";
        private const string PERFORMANCE_REPORT = "playerScores";
        private const string INJURY_REPORT = "injuries";
        private const string PLAYER_REPORT = "players";
        private const int CURRENT_WEEK = 12;

        static void Main(string[] args)
        {
            Task.Run(async () =>
            {
                await GatherInfoAndGenerateAggregateFile();
            }).Wait();
        }
        public static async Task GatherInfoAndGenerateAggregateFile()
        {
            var playerInfo = FetchPlayerInfoData();
            var injuryData = FetchInjuryData();
            var performanceData = FetchPerformanceData();

            await Task.WhenAll(playerInfo, injuryData, performanceData);
            await MergeAndOutputAggregateFile(playerInfo.Result, injuryData.Result, performanceData.Result);
        }

        public static async Task<IEnumerable<PlayerInfo>> FetchPlayerInfoData()
        {
            HttpClient client = new HttpClient();
            var response = await client.GetAsync(string.Format(BASE_ADDRESS, 2016, PLAYER_REPORT, LEAGUE_ID, ""));
            var result = await response.Content.ReadAsStringAsync();
            var jsonResult = JsonConvert.DeserializeObject<PlayerInfoData>(result);
            var toReturn = jsonResult.PlayerList.Players;
            return toReturn;
        }

        public static async Task<IEnumerable<InjuryData>> FetchInjuryData()
        {
            HttpClient client = new HttpClient();
            List<InjuryData> toReturn = new List<InjuryData>();
            for(int i = 1; i < CURRENT_WEEK; i++)
            {
                var response = await client.GetAsync(string.Format(BASE_ADDRESS, 2016, INJURY_REPORT, LEAGUE_ID, i));
                var result = await response.Content.ReadAsStringAsync();
                var jsonResult = JsonConvert.DeserializeObject<InjuryData>(result);
                toReturn.Add(jsonResult);
            }

            return toReturn;
        }

        public static async Task<IEnumerable<PerformanceData>> FetchPerformanceData()
        {
            HttpClient client = new HttpClient();
            List<PerformanceData> toReturn = new List<PerformanceData>();
            for (int i = 1; i < CURRENT_WEEK; i++)
            {
                var response = await client.GetAsync(string.Format(BASE_ADDRESS, 2016, PERFORMANCE_REPORT, LEAGUE_ID, i));
                var result = await response.Content.ReadAsStringAsync();
                var jsonResult = JsonConvert.DeserializeObject<PerformanceData>(result);
                toReturn.Add(jsonResult);
            }

            return toReturn; ;
        }

        public static async Task MergeAndOutputAggregateFile(
            IEnumerable<PlayerInfo> playerInfo,
            IEnumerable<InjuryData> injuryData,
            IEnumerable<PerformanceData> performanceData)
        {
            Dictionary<int, PlayerResult> results = new Dictionary<int, PlayerResult>();

            for (int i = 0; i < CURRENT_WEEK - 1; i++)
            {
                var weeklyInjuryData = injuryData.ElementAt(i);
                foreach(var id in weeklyInjuryData.InjuriesList.Injuries)
                {
                    if(results.ContainsKey(id.PlayerId))
                    {
                        results[id.PlayerId].Injuries.Add(true);
                    }
                    else
                    {
                        var newResult = new PlayerResult()
                        {
                            Id = id.PlayerId
                        };
                        newResult.Injuries.Add(true);
                        results.Add(id.PlayerId, newResult);
                    }
                }

                var weeklyPerformanceData = performanceData.ElementAt(i);
                foreach(var fd in weeklyPerformanceData.PlayerScores.Scores)
                {
                    if (results.ContainsKey(fd.PlayerId))
                    {
                        results[fd.PlayerId].Scores.Add(fd.Score);
                    }
                    else
                    {
                        var newResult = new PlayerResult()
                        {
                            Id = fd.PlayerId
                        };
                        newResult.Scores.Add(fd.Score);
                        results.Add(fd.PlayerId, newResult);
                    }
                }
            }

            foreach(var info in playerInfo)
            {
                if(results.ContainsKey(info.Id))
                {
                    results[info.Id].Name = NormalizeName(info.Name);
                    results[info.Id].Position = info.Position;
                }
            }

            var x = 0;
        }

        private static string NormalizeName(string name)
        {
            // Names appear last name first, reverse it back
            var nameSplit = name.Split(',');
            if(nameSplit.Length == 2)
            {
                name = string.Format("{0} {1}", nameSplit[1].Trim(), nameSplit[0].Trim());
            }

            Regex rgx = new Regex("[^a-z ]+");
            name = rgx.Replace(name.ToLower(), "");
            name = name.Replace(" sr", "").Replace(" jr", "");
            // Check if the name is a defense/city player
            string[] dsts = {
                "Baltimore",
                "Houston",
                "Arizona",
                "Denver",
                "Carolina",
                "Kansas City",
                "Chicago",
                "Jacksonville",
                "New England",
                "New Orleans",
                "Tampa Bay",
                "Los Angeles",
                "Philadelphia",
                "New York Jets",
                "Dallas",
                "Cincinnati",
                "Cleveland",
                "San Diego",
                "Minnesota",
                "Washington",
                "San Francisco",
                "Atlanta",
                "New York Giants",
                "Miami",
                "Green Bay",
                "Seattle",
                "Tennessee",
                "Pittsburgh",
                "Oakland",
                "Detroit",
                "Indianapolis",
                "Buffalo"
            };
            foreach (var d in dsts)
            {
                if (name.Contains(d.ToLower()))
                    name = d.ToLower();
            }
            return name;
        }

    }
}
