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
        private const string NUMBERFIRE_PROJECTIONS_DATA_URI = "http://www.numberfire.com/nfl/daily-fantasy/daily-football-projections";
        private const string REDDIT_BASE_URI = "https://www.reddit.com";
        private const string REDDIT_DEFENSE_URI = "/user/quickonthedrawl/submitted/";
        private const string EMPEOPLED_POST_URI = "https://empeopled.com/discussion?object_type=share&object_id={0}&comment_id=";

        private const int BASE_DEFENSE_BLOG_PROJ = 5;
        private const decimal BLOG_DEFENSE_WEIGHT = .5M;
        private const decimal NUMBERFIRE_DEFENSE_WEIGHT = .5M;

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

            var playerResponse = client.GetAsync(NUMBERFIRE_PROJECTIONS_DATA_URI);
            var nfDefenseResponse = client.GetAsync(NUMBERFIRE_PROJECTIONS_DATA_URI + "/D");
            var redditUserHistoryResponse = client.GetAsync(REDDIT_BASE_URI + REDDIT_DEFENSE_URI);

            await Task.WhenAll(playerResponse, nfDefenseResponse, redditUserHistoryResponse);

            var playerResult = await playerResponse.Result.Content.ReadAsStringAsync();
            HtmlDocument playerDoc = new HtmlDocument();
            playerDoc.LoadHtml(playerResult);
            IEnumerable<HtmlNode> tableNodes = playerDoc.DocumentNode.SelectNodes("//tbody").Where(n => n.ChildNodes.Count > 1);

            var nfDefenseResult = await nfDefenseResponse.Result.Content.ReadAsStringAsync();
            HtmlDocument nfDefenseDoc = new HtmlDocument();
            nfDefenseDoc.LoadHtml(nfDefenseResult);
            IEnumerable<HtmlNode> nfDefenseTableNodes = nfDefenseDoc.DocumentNode.SelectNodes("//tbody").Where(n => n.ChildNodes.Count > 1);

            // For reddit projections we have to go down the following pattern:
            // 1. Find the reddit post
            // 2. Fetch the post, find the link to the blog post
            // 3. Go to the blog post & fetch projection data
            // ** (1) **
            var redditUserHistoryResult = await redditUserHistoryResponse.Result.Content.ReadAsStringAsync();
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

            // Zip the data together and return
            var playerInfo = tableNodes.ElementAt(0).Descendants("tr");
            var projectionsInfo = tableNodes.ElementAt(1).Descendants("tr");
            var zippedInfo = playerInfo.Zip(projectionsInfo, (info, proj) => new { info, proj});

            var nfDefenseInfo = nfDefenseTableNodes.ElementAt(0).Descendants("tr");
            var nfDefenseProjectionInfo = nfDefenseTableNodes.ElementAt(1).Descendants("tr");
            var zippedNfDefenseInfo = nfDefenseInfo.Zip(nfDefenseProjectionInfo, (info, proj) => new { info, proj });
            zippedInfo = zippedInfo.Concat(zippedNfDefenseInfo);

            List<PlayerProjectionData> playerData = new List<PlayerProjectionData>();
            List<PlayerProjectionData> defenseData = new List<PlayerProjectionData>();
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

                var playerProjectionData = new PlayerProjectionData()
                {
                    Name = NormalizeName(playerNameText),
                    Projection = Convert.ToDecimal(projectionText),
                    Position = positionText
                };
                if (positionText == "def")
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
                    Position = "def"
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
