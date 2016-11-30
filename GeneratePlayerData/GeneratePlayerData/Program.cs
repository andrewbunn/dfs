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
        private const string NUMBERFIRE_PROJECTIONS_DATA_URI = "https://www.numberfire.com/nfl/fantasy/fantasy-football-projections/{0}";
        private const string REDDIT_BASE_URI = "https://www.reddit.com";
        private const string REDDIT_DEFENSE_URI = "/user/quickonthedrawl/submitted/";
        private const string EMPEOPLED_POST_URI = "https://empeopled.com/discussion?object_type=share&object_id={0}&comment_id=";

        private const int BASE_DEFENSE_BLOG_PROJ = 5;
        private const decimal BLOG_DEFENSE_WEIGHT = .5M;
        private const decimal NUMBERFIRE_DEFENSE_WEIGHT = .5M;

        private static readonly string[] NUMBERFIRE_POSITION_URL_SUFFIXES = { "qb", "rb", "wr", "te", "d" };

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
            var projectionsData = FetchProjectionsData();

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

        public static async Task<IEnumerable<PlayerProjectionData>> FetchProjectionsData()
        {
            // Gather information from Numberfire tables
            HttpClient client = new HttpClient();
            List<Task<HttpResponseMessage>> httpResponses = new List<Task<HttpResponseMessage>>();
            foreach(var pos in NUMBERFIRE_POSITION_URL_SUFFIXES)
            {
                httpResponses.Add(client.GetAsync(string.Format(NUMBERFIRE_PROJECTIONS_DATA_URI, pos)));
            }

            // Add the call for the reddit thread response
            httpResponses.Add(client.GetAsync(REDDIT_BASE_URI + REDDIT_DEFENSE_URI));

            await Task.WhenAll(httpResponses);

            NumberFireZippedPlayerInfo zippedInfo = new NumberFireZippedPlayerInfo();
            // Don't include the last added response, process is different for the reddit post.
            for(int i = 0; i < httpResponses.Count - 1; i++)
            {
                var playerResult = await httpResponses[i].Result.Content.ReadAsStringAsync();
                HtmlDocument playerDoc = new HtmlDocument();
                playerDoc.LoadHtml(playerResult);
                var nodes = playerDoc.DocumentNode.SelectNodes("//tbody").Where(n => n.ChildNodes.Count > 1);
                var playerInfoTable = nodes.ElementAt(0).Descendants("tr");
                var projectionsInfoTable = nodes.ElementAt(1).Descendants("tr");
                zippedInfo.Info.AddRange(playerInfoTable);
                zippedInfo.Projections.AddRange(projectionsInfoTable);
                zippedInfo.Position.AddRange(
                    Enumerable.Repeat(TranslateToPositionEnum(NUMBERFIRE_POSITION_URL_SUFFIXES[i]), playerInfoTable.Count()));
            }

            // For reddit projections we have to go down the following pattern:
            // 1. Find the reddit post
            // 2. Fetch the post, find the link to the blog post
            // 3. Go to the blog post & fetch projection data
            // ** (1) **
            var redditUserHistoryResult = await httpResponses[httpResponses.Count-1].Result.Content.ReadAsStringAsync();
            HtmlDocument userHistoryDoc = new HtmlDocument();
            userHistoryDoc.LoadHtml(redditUserHistoryResult);
            var topPost = userHistoryDoc.DocumentNode.SelectSingleNode("//div[contains(@id,'siteTable')]/div[contains(@class,'thing')]");
            var topPostUrl = REDDIT_BASE_URI + topPost.Attributes["data-url"].Value;

            // ** (2) **
            var redditPostResponse = await client.GetAsync(topPostUrl);
            var redditPostResult = await redditPostResponse.Content.ReadAsStringAsync();
            HtmlDocument redditPostDoc = new HtmlDocument();
            redditPostDoc.LoadHtml(redditPostResult);
            var blogPost = redditPostDoc.DocumentNode.SelectSingleNode(
                "//a[contains(@href,'empeopled') and contains(.,'Defense Wins Championships')] ");
            var blogPostUrl = blogPost.Attributes["href"].Value;
            var blogPostId = blogPostUrl.Split('/').Last();

            // ** (3) **
            var blogPostResponse = await client.GetAsync(string.Format(EMPEOPLED_POST_URI, blogPostId));
            var blogPostResult = await blogPostResponse.Content.ReadAsStringAsync();
            var blogData = JsonConvert.DeserializeObject<EmpeopledBlogPostResponse>(blogPostResult);
            HtmlDocument blogPostDoc = new HtmlDocument();
            blogPostDoc.LoadHtml(blogData.Data.Content.Body);
            var blogDefenseNodes = blogPostDoc.DocumentNode.SelectNodes("//ol/li");

            List<PlayerProjectionData> playerData = new List<PlayerProjectionData>();
            List<PlayerProjectionData> defenseData = new List<PlayerProjectionData>();
            for(int i = 0; i < zippedInfo.Info.Count; i++)
            {
                var playerName = zippedInfo.Info[i].Descendants("span")
                    .Where(a => a.Attributes["class"].Value.Contains("full")).FirstOrDefault();
                var projection = zippedInfo.Projections[i].Descendants("td")
                    .Where(a => a.HasAttributes && a.Attributes["class"] != null && a.Attributes["class"].Value.Contains("fanduel_fp")).FirstOrDefault();
                var confidenceText = zippedInfo.Projections[i].SelectSingleNode("td[2]").InnerText.Trim();
                var position = zippedInfo.Position[i];
                var playerNameText = playerName != null ? playerName.InnerText.Trim() : "";
                var projectionValue = projection != null ? Convert.ToDecimal(projection.InnerText.Trim()) : 0M;
                var standardDevText = confidenceText.Split('-');
                var standardDev = standardDevText.Length == 2 ? 
                    projectionValue - Convert.ToDecimal(standardDevText[0]) :
                    projectionValue - Convert.ToDecimal(standardDevText[1]) * -1;

                var playerProjectionData = new PlayerProjectionData()
                {
                    Name = NormalizeName(playerNameText),
                    Projection = projectionValue,
                    Position = position.ToString(),
                    StandardDeviation = standardDev
                };
                if (position == PositionEnum.def)
                    defenseData.Add(playerProjectionData);
                else
                    playerData.Add(playerProjectionData);
            }

            // Now translate the defense data from the blog post and merge with the existing defense information
            List<PlayerProjectionData> blogDefenseData = new List<PlayerProjectionData>();
            foreach(var blogDefenseRow in blogDefenseNodes)
            {
                var splitRow = blogDefenseRow.InnerText.Split(',');
                var cityName = splitRow[0].Trim();
                var splitProj = splitRow[1].Split('–');
                var projection = splitProj[0].Trim();
                blogDefenseData.Add(new PlayerProjectionData()
                {
                    Name = NormalizeName(cityName),
                    Projection = Convert.ToDecimal(projection),
                    Position = PositionEnum.def.ToString()
                });
            }

            // Foreach existing defense see if there is a blog post corresponding, if not use the base.
            foreach(var defense in defenseData)
            {
                var blogMatch = blogDefenseData.Where(b => b.Name == defense.Name).FirstOrDefault();
                if (blogMatch != null)
                    defense.Projection = (blogMatch.Projection * BLOG_DEFENSE_WEIGHT) + (defense.Projection * NUMBERFIRE_DEFENSE_WEIGHT);
                else
                    defense.Projection = (BASE_DEFENSE_BLOG_PROJ * BLOG_DEFENSE_WEIGHT) + (defense.Projection * NUMBERFIRE_DEFENSE_WEIGHT);
            }
            playerData.AddRange(defenseData);
            
            return playerData;
        }

        public static async Task MergeDataAndOutputResults(
            IEnumerable<YahooPlayerData> yahooPlayerData,
            IEnumerable<PlayerProjectionData> projectionData,
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
                        var matchingPlayer = projectionData.Where(
                            n => n.Name == player.Name && 
                            (!string.IsNullOrEmpty(n.Position) ? 
                            n.PositionEnum == player.PositionEnum : true)).FirstOrDefault();

                        var exclude = !includeThursdayPlayers && player.GameStartTime.DayOfWeek == DayOfWeek.Thursday;
                        var gameFinished = player.GameStartTime < DateTime.Now;
                        if (matchingPlayer != null && !exclude && !gameFinished)
                        {
                            var projectedPoints = matchingPlayer.Projection;
                            var stdDeviation = matchingPlayer.StandardDeviation;
                            var projectionAlteration = projectionAlterations.Where(pa => pa.Name == pa.Name).FirstOrDefault();
                            if (projectionAlteration != null)
                            {
                                projectedPoints += projectionAlteration.Difference;
                            }

                            var output = string.Format("{0},{1},{2},{3},{4},",
                                player.Name,
                                player.Salary,
                                projectedPoints,
                                (int)player.PositionEnum,
                                stdDeviation);
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

        private static PositionEnum TranslateToPositionEnum(string position)
        {
            switch(position)
            {
                case "qb":
                    return PositionEnum.qb;
                case "rb":
                    return PositionEnum.rb;
                case "wr":
                    return PositionEnum.wr;
                case "te":
                    return PositionEnum.te;
                case "d":
                default:
                    return PositionEnum.def;
            }
        }
    }
}
