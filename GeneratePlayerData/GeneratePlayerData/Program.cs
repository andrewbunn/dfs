using HtmlAgilityPack;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Diagnostics;

namespace GeneratePlayerData
{
    class Program
    {
        private const string YAHOO_PLAYER_DATA_URI = "https://dfyql-ro.sports.yahoo.com/v2/external/playersFeed/nfl";
        private const string PROJECTIONS_DATA_URI = "http://www.numberfire.com/nfl/daily-fantasy/daily-football-projections";

        static void Main(string[] args)
        {
            bool includeThursdayGames = args.Length > 0 ? Convert.ToBoolean(args[0]) : true;
            string diffFilePath = args.Length > 1 ? args[1] : "";
            Task.Run(async () =>
            {
                await GatherAndGeneratePlayerFile(includeThursdayGames, diffFilePath);
            }).Wait();
        }

        public static async Task GatherAndGeneratePlayerFile(bool includeThursdayGames, string diffFilePath)
        {
            var yahooPlayerData = FetchYahooPlayerData();
            var projectionsData = FetchNumberfireProjectionsData();

            await Task.WhenAll(yahooPlayerData, projectionsData);
            await MergeDataAndOutputResults(
                yahooPlayerData.Result, 
                projectionsData.Result, 
                includeThursdayGames,
                diffFilePath);
        }

        public static async Task<IEnumerable<YahooPlayerData>> FetchYahooPlayerData()
        {
            HttpClient client = new HttpClient();
            var response = await client.GetAsync(YAHOO_PLAYER_DATA_URI);
            var result = await response.Content.ReadAsStringAsync();
            var jsonResult = JsonConvert.DeserializeObject<YahooResponse>(result);
            var toReturn = jsonResult.Result.Players;
            foreach(var player in toReturn)
            {
                player.Name = NormalizeName(player.Name);
            }
            return jsonResult.Result.Players;
        }

        public static async Task<IEnumerable<NumberfirePlayerData>> FetchNumberfireProjectionsData()
        {
            HttpClient client = new HttpClient();
            var playerResponse = await client.GetAsync(PROJECTIONS_DATA_URI);
            var playerResult = await playerResponse.Content.ReadAsStringAsync();
            var defenseResponse = await client.GetAsync(PROJECTIONS_DATA_URI + "/D");
            var defenseResult = await defenseResponse.Content.ReadAsStringAsync();
            HtmlDocument playerDoc = new HtmlDocument();
            playerDoc.LoadHtml(playerResult);
            HtmlDocument defenseDoc = new HtmlDocument();
            defenseDoc.LoadHtml(defenseResult);

            // Find the two correct tables, both are the only tbodys with children on the page.
            IEnumerable<HtmlNode> tableNodes = playerDoc.DocumentNode.SelectNodes("//tbody").Where(n => n.ChildNodes.Count > 1);
            IEnumerable<HtmlNode> defenseTableNodes = defenseDoc.DocumentNode.SelectNodes("//tbody").Where(n => n.ChildNodes.Count > 1);

            // Iterate through the two tables constructing the player objects.
            var playerInfo = tableNodes.ElementAt(0).Descendants("tr");
            var projectionsInfo = tableNodes.ElementAt(1).Descendants("tr");
            var zippedInfo = playerInfo.Zip(projectionsInfo, (info, proj) => new { info, proj});

            var defenseInfo = defenseTableNodes.ElementAt(0).Descendants("tr");
            var defenseProjectionInfo = defenseTableNodes.ElementAt(1).Descendants("tr");
            var zippedDefenseInfo = defenseInfo.Zip(defenseProjectionInfo, (info, proj) => new { info, proj });
            zippedInfo = zippedInfo.Concat(zippedDefenseInfo);

            List<NumberfirePlayerData> playerData = new List<NumberfirePlayerData>();
            foreach(var playerRow in zippedInfo)
            {
                // If our rows somehow become unaligned throw an exception
                if(playerRow.info.Attributes["data-player-id"].Value != playerRow.proj.Attributes["data-player-id"].Value)
                {
                    throw new Exception("Data Player IDs unaligned.");
                }
                var playerName = playerRow.info.Descendants("a")
                    .Where(a => a.Attributes["class"].Value.Contains("full")).FirstOrDefault();
                var position = playerRow.info.Descendants("span")
                    .Where(a => a.Attributes["class"].Value.Contains("player-info--position")).FirstOrDefault();
                var projection = playerRow.proj.Descendants("td")
                    .Where(a => a.Attributes["class"].Value.Contains("fp")).FirstOrDefault();
                var playerNameText = playerName != null ? playerName.InnerText.Trim() : "";
                var positionText = position != null ? position.InnerText.Trim() : "def";
                var projectionText = projection != null ? projection.InnerText.Trim() : "0";

                playerData.Add(new NumberfirePlayerData()
                {
                    Name = NormalizeName(playerNameText),
                    Projection = Convert.ToDecimal(projectionText),
                    Position = positionText
                });
            }
            return playerData;
        }

        public static async Task MergeDataAndOutputResults(
            IEnumerable<YahooPlayerData> yahooPlayerData,
            IEnumerable<NumberfirePlayerData> numberfireData,
            bool includeThursdayPlayers,
            string diffFilePath)
        {
            List<PlayerProjectionAlteration> projectionAlterations = new List<PlayerProjectionAlteration>();
            if (!string.IsNullOrEmpty(diffFilePath))
            {
                using (var reader = new StreamReader(diffFilePath))
                {
                    string line;
                    while((line = reader.ReadLine()) != null)
                    {
                        var projAlterItems = line.Split(',');
                        projectionAlterations.Add(new PlayerProjectionAlteration()
                        {
                            Name = NormalizeName(projAlterItems[0]),
                            Difference = Convert.ToDecimal(projAlterItems[1])
                        });
                    }
                }
            }

            using (FileStream fs = File.Open("mergedPlayers.csv", FileMode.Create))
            {
                using (var writer = new StreamWriter(fs))
                {
                    foreach (var player in yahooPlayerData)
                    {
                        // For some reason numberfire has missing position data so in those cases, just match based on name
                        var matchingPlayer = numberfireData.Where(
                            n => n.Name == player.Name && 
                            (!string.IsNullOrEmpty(n.Position) ? 
                            n.PositionEnum == player.PositionEnum : true)).FirstOrDefault();

                        var exclude = !includeThursdayPlayers && player.GameStartTime.DayOfWeek == DayOfWeek.Thursday;
                        if (matchingPlayer != null && !exclude)
                        {
                            var projectedPoints = matchingPlayer.Projection;
                            var projectionAlteration = projectionAlterations.Where(pa => pa.Name == pa.Name).FirstOrDefault();
                            if (projectionAlteration != null)
                            {
                                projectedPoints += projectionAlteration.Difference;
                            }

                            var output = string.Format("{0},{1},{2},{3},",
                                player.Name,
                                player.Salary,
                                projectedPoints,
                                (int)player.PositionEnum);
                            writer.WriteLine(output);
                        }
                    }
                }
            }
        }

        private static string NormalizeName(string name)
        {
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
            foreach(var d in dsts)
            {
                if (name.Contains(d.ToLower()))
                    name = d.ToLower();
            }

            return name;
        }
    }
}
