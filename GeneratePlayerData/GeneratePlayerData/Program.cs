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
            Task.Run(async () =>
            {
                await GatherAndGeneratePlayerFile();
            }).Wait();
        }

        public static async Task GatherAndGeneratePlayerFile()
        {
            var yahooPlayerData = FetchYahooPlayerData();
            var projectionsData = FetchNumberfireProjectionsData();

            await Task.WhenAll(yahooPlayerData, projectionsData);
            await MergeDataAndOutputResults(yahooPlayerData.Result, projectionsData.Result);
        }

        public static async Task<IEnumerable<YahooPlayerData>> FetchYahooPlayerData()
        {
            HttpClient client = new HttpClient();
            var response = await client.GetAsync(YAHOO_PLAYER_DATA_URI);
            var result = await response.Content.ReadAsStringAsync();
            var jsonResult = JsonConvert.DeserializeObject<YahooResponse>(result);
            return jsonResult.Result.Players;
        }

        public static async Task<IEnumerable<NumberfirePlayerData>> FetchNumberfireProjectionsData()
        {
            HttpClient client = new HttpClient();
            var response = await client.GetAsync(PROJECTIONS_DATA_URI);
            var result = await response.Content.ReadAsStringAsync();
            HtmlDocument doc = new HtmlDocument();
            doc.LoadHtml(result);
            // Find the two correct tables, both are the only tbodys with children on the page.
            IEnumerable<HtmlNode> tableNodes = doc.DocumentNode.SelectNodes("//tbody").Where(n => n.ChildNodes.Count > 1);

            // Iterate through the two tables constructing the player objects.
            var playerInfo = tableNodes.ElementAt(0).Descendants("tr");
            var projectionsInfo = tableNodes.ElementAt(1).Descendants("tr");
            var zippedInfo = playerInfo.Zip(projectionsInfo, (info, proj) => new { info, proj});
            List<NumberfirePlayerData> playerData = new List<NumberfirePlayerData>();
            foreach(var playerRow in zippedInfo)
            {
                // If our rows somehow become unaligned throw an exception
                if(playerRow.info.Attributes["data-player-id"].Value != playerRow.proj.Attributes["data-player-id"].Value)
                {
                    throw new Exception("Data Player IDs unaligned.");
                }
                string playerName = playerRow.info.Descendants("a")
                    .Where(a => a.Attributes["class"].Value.Contains("full")).First().InnerText.Trim();
                string position = playerRow.info.Descendants("span")
                    .Where(a => a.Attributes["class"].Value.Contains("player-info--position")).First().InnerText.Trim();
                string projection = playerRow.proj.Descendants("td")
                    .Where(a => a.Attributes["class"].Value.Contains("fp")).First().InnerText.Trim();
                playerData.Add(new NumberfirePlayerData()
                {
                    Name = playerName,
                    Projection = Convert.ToDecimal(projection),
                    Position = position
                });
            }
            return playerData;
        }

        public static async Task MergeDataAndOutputResults(
            IEnumerable<YahooPlayerData> yahooPlayerData,
            IEnumerable<NumberfirePlayerData> numberfireData)
        {
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
                        if (matchingPlayer != null)
                        {
                            var output = string.Format("{0},{1},{2},{3},",
                                NormalizeName(player.Name),
                                player.Salary,
                                matchingPlayer.Projection,
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
            return name;
        }
    }
}
