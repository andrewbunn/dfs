using HtmlAgilityPack;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Data.SqlClient;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace PlayerDataAggregator
{
    class Program
    {
        private enum PositionEnum
        {
            qb = 0,
            rb = 1,
            wr = 2,
            te = 3,
            def = 4
        };

        private const string CONNECTION_STRING = "Server=tcp:dfs-1.database.windows.net,1433;Initial Catalog=dfs;Persist Security Info=False;User ID=bunn_tj_dfs;Password=spartan.99;MultipleActiveResultSets=True;Encrypt=True;TrustServerCertificate=False;Connection Timeout=30;";
        private const int MY_FANTASY_LEAGUE_ID = 11289;
        private const int FF_TODAY_LEAGUE_ID = 186085;

        // Year, ReportType, LeagueID, Week
        private const string MY_FANTASY_BASE_ADDRESS = "http://www70.myfantasyleague.com/{0}/export?TYPE={1}&L={2}&W={3}&JSON=1";
        // Year, Week, PositionId, LeagueID
        private const string FF_TODAY_BASE_ADDRESS = "http://www.fftoday.com/stats/playerstats.php?Season={0}&GameWeek={1}&PosID={2}&LeagueID={3}";
        private const string PERFORMANCE_REPORT = "playerScores";
        private const string INJURY_REPORT = "injuries";
        private const string PLAYER_REPORT = "players";
        private const int CURRENT_WEEK = 13;

        private static readonly Dictionary<int, string> FF_TODAY_POSITIONS = new Dictionary<int, string>()
        {
            { 10, "qb" }, { 20, "rb" }, { 30, "wr" }, { 40, "te" }, { 50, "def" }
        };

        private static List<Tuple<int, int, KeyValuePair<int, string>>> DataRequests = new List<Tuple<int, int, KeyValuePair<int, string>>>();

        static void Main(string[] args)
        {
            Task.Run(async () =>
            {
                //await GatherInfoAndGenerateAggregateFile();
                for (int i = 2000; i < 2017; i++)
                {
                    foreach (var pos in FF_TODAY_POSITIONS)
                    {
                        for (int j = 1; j <= 17; j++)
                        {
                            DataRequests.Add(new Tuple<int, int, KeyValuePair<int, string>>(i, j, pos));
                        }
                    }
                }
                do
                {
                    await GatherHistoricalDataAndStore();
                } while (DataRequests.Count > 0);
                Console.WriteLine("DONE!");
                Console.ReadLine();
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

        public static async Task GatherHistoricalDataAndStore()
        {
            // Gather information from Numberfire tables
            HttpClient client = new HttpClient();
            var failedRequests = new List<Tuple<int, int, KeyValuePair<int, string>>>();
            foreach (var request in DataRequests)
            {
                try
                {
                    var year = request.Item1;
                    var week = request.Item2;
                    var pos = request.Item3;
                    var httpResponse = await client.GetAsync(string.Format(FF_TODAY_BASE_ADDRESS, year, week, pos.Key, FF_TODAY_LEAGUE_ID));
                    var playerResult = await httpResponse.Content.ReadAsStringAsync();
                    HtmlDocument playerDoc = new HtmlDocument();
                    playerDoc.LoadHtml(playerResult);
                    var table = playerDoc.DocumentNode.SelectSingleNode("/html[1]/body[1]/center[1]/table[2]/tr[2]/td[1]/table[6]/tr[1]/td[1]/table[1]");
                    var playerRows = table.Descendants("tr").Where(r => !r.Attributes.Contains("class"));
                    switch (pos.Value)
                    {
                        case "qb":
                            HandleQBData(playerRows.ToList(), week, year);
                            break;
                        case "rb":
                            HandleRBData(playerRows.ToList(), week, year);
                            break;
                        case "wr":
                            HandleWRData(playerRows.ToList(), week, year);
                            break;
                        case "te":
                            HandleTEData(playerRows.ToList(), week, year);
                            break;
                        case "def":
                            HandleDefData(playerRows.ToList());
                            break;
                    }
                }
                catch(Exception ex)
                {
                    Console.WriteLine(string.Format("Failure - Year: {0}, Week: {1}, Pos: {2}", request.Item1, request.Item2, request.Item3));
                    failedRequests.Add(request);
                }
            }
            // Bubble the failed requests back up so unlimited additional attempts can be made.
            DataRequests = failedRequests;
        }

        private static void HandleQBData(List<HtmlNode> playerRows, int week, int year)
        {
            foreach (var row in playerRows)
            {
                var values = row.Descendants("td").ToArray();
                var trimmedValues = new List<string>();
                foreach (var val in values)
                    trimmedValues.Add(Regex.Replace(val.InnerText, @"<[^>]+>|&nbsp;", "").Trim());
                var playerName = NormalizeName(trimmedValues[0]);
                var team = trimmedValues[1].ToLower();
                var numCompletions = trimmedValues[3];
                var passAttempts = trimmedValues[4];
                var passYards = trimmedValues[5];
                var passTds = trimmedValues[6];
                var ints = trimmedValues[7];
                var rushAttempts = trimmedValues[8];
                var rushYards = trimmedValues[9];
                var rushTds = trimmedValues[10];
                var fantasyPoints = trimmedValues[11];
                Nullable<int> playerId = null;

                // Attempt to find a player object with the fetched name. If it exists, use that player id.
                // If it doesnt, create the new player and use its new id
                using (SqlConnection conn = new SqlConnection(CONNECTION_STRING))
                {
                    string queryString = "SELECT id FROM  [dbo].[players] WHERE name = @playerName AND position = @position";
                    SqlCommand command = new SqlCommand(queryString, conn);
                    command.Parameters.AddWithValue("@playerName", playerName);
                    command.Parameters.AddWithValue("@position", ((int)PositionEnum.qb).ToString());
                    conn.Open();
                    SqlDataReader reader = command.ExecuteReader();
                    try
                    {
                        if(reader.HasRows)
                        {
                            while (reader.Read())
                            {
                                playerId = (int)reader["id"];
                            }
                        }
                        else
                        {
                            // There are no player names with the matching name, insert a new row.
                            string insertPlayerQuery = "INSERT INTO [dbo].[players](name, position) output INSERTED.ID VALUES (@playerName, @position)";
                            SqlCommand insertPlayerCommand = new SqlCommand(insertPlayerQuery, conn);
                            insertPlayerCommand.Parameters.AddWithValue("@playerName", playerName);
                            insertPlayerCommand.Parameters.AddWithValue("@position", ((int)PositionEnum.qb).ToString());
                            var result = insertPlayerCommand.ExecuteScalar();
                            playerId = (int)result;
                        }
                    }
                    catch(Exception ex)
                    {
                        var x = ex;
                    }
                    finally
                    {
                        // Always call Close when done reading.
                        reader.Close();
                    }

                    if(playerId.HasValue)
                    {
                        string insertPerformanceQuery = 
                            "INSERT INTO [dbo].[player_performances](player_id, score_value, nfl_week, year, team, rushes, " +
                            "rushing_yards, rushing_tds, passing_attempts, passing_completions, passing_yards, passing_tds, passing_ints) " +
                            " VALUES (@playerId, @score, @week, @year, @team, @rushes, @r_yards, @r_tds, @passes, @completions, @p_yards, @p_tds, @p_ints)";
                        SqlCommand insertPerformanceCommand = new SqlCommand(insertPerformanceQuery, conn);
                        insertPerformanceCommand.Parameters.AddWithValue("@playerId", playerId.Value);
                        insertPerformanceCommand.Parameters.AddWithValue("@score", fantasyPoints);
                        insertPerformanceCommand.Parameters.AddWithValue("@week", week);
                        insertPerformanceCommand.Parameters.AddWithValue("@year", year);
                        insertPerformanceCommand.Parameters.AddWithValue("@team", team);
                        insertPerformanceCommand.Parameters.AddWithValue("@rushes", rushAttempts);
                        insertPerformanceCommand.Parameters.AddWithValue("@r_yards", rushYards);
                        insertPerformanceCommand.Parameters.AddWithValue("@r_tds", rushTds);
                        insertPerformanceCommand.Parameters.AddWithValue("@passes", passAttempts);
                        insertPerformanceCommand.Parameters.AddWithValue("@completions", numCompletions);
                        insertPerformanceCommand.Parameters.AddWithValue("@p_yards", passYards);
                        insertPerformanceCommand.Parameters.AddWithValue("@p_tds", passTds);
                        insertPerformanceCommand.Parameters.AddWithValue("@p_ints", ints);
                        var rowInserted = insertPerformanceCommand.ExecuteNonQuery() > 0;
                    }
                    else
                    {
                        // If no playerId is present something went wrong.
                        throw new Exception("No Player ID for QB");
                    }

                    conn.Close();
                }
            }
        }

        private static void HandleRBData(List<HtmlNode> playerRows, int week, int year)
        {
            foreach (var row in playerRows)
            {
                var values = row.Descendants("td").ToArray();
                var trimmedValues = new List<string>();
                foreach (var val in values)
                    trimmedValues.Add(Regex.Replace(val.InnerText, @"<[^>]+>|&nbsp;", "").Trim());
                var playerName = NormalizeName(trimmedValues[0]);
                var team = trimmedValues[1].ToLower();
                var rushAttempts = trimmedValues[3];
                var rushYards = trimmedValues[4];
                var rushTds = trimmedValues[5];
                var targets = trimmedValues[6];
                var receptions = trimmedValues[7];
                var recYards = trimmedValues[8];
                var recTds = trimmedValues[9];
                var fantasyPoints = trimmedValues[10];
                Nullable<int> playerId = null;

                // Attempt to find a player object with the fetched name. If it exists, use that player id.
                // If it doesnt, create the new player and use its new id
                using (SqlConnection conn = new SqlConnection(CONNECTION_STRING))
                {
                    string queryString = "SELECT id FROM  [dbo].[players] WHERE name = @playerName AND position = @position";
                    SqlCommand command = new SqlCommand(queryString, conn);
                    command.Parameters.AddWithValue("@playerName", playerName);
                    command.Parameters.AddWithValue("@position", ((int)PositionEnum.qb).ToString());
                    conn.Open();
                    SqlDataReader reader = command.ExecuteReader();
                    try
                    {
                        if (reader.HasRows)
                        {
                            while (reader.Read())
                            {
                                playerId = (int)reader["id"];
                            }
                        }
                        else
                        {
                            // There are no player names with the matching name, insert a new row.
                            string insertPlayerQuery = "INSERT INTO [dbo].[players](name, position) output INSERTED.ID VALUES (@playerName, @position)";
                            SqlCommand insertPlayerCommand = new SqlCommand(insertPlayerQuery, conn);
                            insertPlayerCommand.Parameters.AddWithValue("@playerName", playerName);
                            insertPlayerCommand.Parameters.AddWithValue("@position", ((int)PositionEnum.rb).ToString());
                            var result = insertPlayerCommand.ExecuteScalar();
                            playerId = (int)result;
                        }
                    }
                    catch (Exception ex)
                    {
                        var x = ex;
                    }
                    finally
                    {
                        // Always call Close when done reading.
                        reader.Close();
                    }

                    if (playerId.HasValue)
                    {
                        string insertPerformanceQuery =
                            "INSERT INTO [dbo].[player_performances](player_id, score_value, nfl_week, year, team, rushes, " +
                            "rushing_yards, rushing_tds, targets, receptions, receiving_yards, receiving_tds) " +
                            " VALUES (@playerId, @score, @week, @year, @team, @rushes, @r_yards, @r_tds, @targets, @receptions, @receiving_yards, @receiving_tds)";
                        SqlCommand insertPerformanceCommand = new SqlCommand(insertPerformanceQuery, conn);
                        insertPerformanceCommand.Parameters.AddWithValue("@playerId", playerId.Value);
                        insertPerformanceCommand.Parameters.AddWithValue("@score", fantasyPoints);
                        insertPerformanceCommand.Parameters.AddWithValue("@week", week);
                        insertPerformanceCommand.Parameters.AddWithValue("@year", year);
                        insertPerformanceCommand.Parameters.AddWithValue("@team", team);
                        insertPerformanceCommand.Parameters.AddWithValue("@rushes", rushAttempts);
                        insertPerformanceCommand.Parameters.AddWithValue("@r_yards", rushYards);
                        insertPerformanceCommand.Parameters.AddWithValue("@r_tds", rushTds);
                        insertPerformanceCommand.Parameters.AddWithValue("@targets", targets);
                        insertPerformanceCommand.Parameters.AddWithValue("@receptions", receptions);
                        insertPerformanceCommand.Parameters.AddWithValue("@receiving_yards", recYards);
                        insertPerformanceCommand.Parameters.AddWithValue("@receiving_tds", recTds);
                        var rowInserted = insertPerformanceCommand.ExecuteNonQuery() > 0;
                    }
                    else
                    {
                        // If no playerId is present something went wrong.
                        throw new Exception("No Player ID for RB");
                    }

                    conn.Close();
                }
            }
        }

        private static void HandleWRData(List<HtmlNode> playerRows, int week, int year)
        {
            foreach (var row in playerRows)
            {
                var values = row.Descendants("td").ToArray();
                var trimmedValues = new List<string>();
                foreach (var val in values)
                    trimmedValues.Add(Regex.Replace(val.InnerText, @"<[^>]+>|&nbsp;", "").Trim());
                var playerName = NormalizeName(trimmedValues[0]);
                var team = trimmedValues[1].ToLower();
                var rushAttempts = trimmedValues[3];
                var rushYards = trimmedValues[4];
                var rushTds = trimmedValues[5];
                var targets = trimmedValues[6];
                var receptions = trimmedValues[7];
                var recYards = trimmedValues[8];
                var recTds = trimmedValues[9];
                var fantasyPoints = trimmedValues[10];
                Nullable<int> playerId = null;

                // Attempt to find a player object with the fetched name. If it exists, use that player id.
                // If it doesnt, create the new player and use its new id
                using (SqlConnection conn = new SqlConnection(CONNECTION_STRING))
                {
                    string queryString = "SELECT id FROM  [dbo].[players] WHERE name = @playerName AND position = @position";
                    SqlCommand command = new SqlCommand(queryString, conn);
                    command.Parameters.AddWithValue("@playerName", playerName);
                    command.Parameters.AddWithValue("@position", ((int)PositionEnum.qb).ToString());
                    conn.Open();
                    SqlDataReader reader = command.ExecuteReader();
                    try
                    {
                        if (reader.HasRows)
                        {
                            while (reader.Read())
                            {
                                playerId = (int)reader["id"];
                            }
                        }
                        else
                        {
                            // There are no player names with the matching name, insert a new row.
                            string insertPlayerQuery = "INSERT INTO [dbo].[players](name, position) output INSERTED.ID VALUES (@playerName, @position)";
                            SqlCommand insertPlayerCommand = new SqlCommand(insertPlayerQuery, conn);
                            insertPlayerCommand.Parameters.AddWithValue("@playerName", playerName);
                            insertPlayerCommand.Parameters.AddWithValue("@position", ((int)PositionEnum.wr).ToString());
                            var result = insertPlayerCommand.ExecuteScalar();
                            playerId = (int)result;
                        }
                    }
                    catch (Exception ex)
                    {
                        var x = ex;
                    }
                    finally
                    {
                        // Always call Close when done reading.
                        reader.Close();
                    }

                    if (playerId.HasValue)
                    {
                        string insertPerformanceQuery =
                            "INSERT INTO [dbo].[player_performances](player_id, score_value, nfl_week, year, team, rushes, " +
                            "rushing_yards, rushing_tds, targets, receptions, receiving_yards, receiving_tds) " +
                            " VALUES (@playerId, @score, @week, @year, @team, @rushes, @r_yards, @r_tds, @targets, @receptions, @receiving_yards, @receiving_tds)";
                        SqlCommand insertPerformanceCommand = new SqlCommand(insertPerformanceQuery, conn);
                        insertPerformanceCommand.Parameters.AddWithValue("@playerId", playerId.Value);
                        insertPerformanceCommand.Parameters.AddWithValue("@score", fantasyPoints);
                        insertPerformanceCommand.Parameters.AddWithValue("@week", week);
                        insertPerformanceCommand.Parameters.AddWithValue("@year", year);
                        insertPerformanceCommand.Parameters.AddWithValue("@team", team);
                        insertPerformanceCommand.Parameters.AddWithValue("@rushes", rushAttempts);
                        insertPerformanceCommand.Parameters.AddWithValue("@r_yards", rushYards);
                        insertPerformanceCommand.Parameters.AddWithValue("@r_tds", rushTds);
                        insertPerformanceCommand.Parameters.AddWithValue("@targets", targets);
                        insertPerformanceCommand.Parameters.AddWithValue("@receptions", receptions);
                        insertPerformanceCommand.Parameters.AddWithValue("@receiving_yards", recYards);
                        insertPerformanceCommand.Parameters.AddWithValue("@receiving_tds", recTds);
                        var rowInserted = insertPerformanceCommand.ExecuteNonQuery() > 0;
                    }
                    else
                    {
                        // If no playerId is present something went wrong.
                        throw new Exception("No Player ID for WR");
                    }

                    conn.Close();
                }
            }
        }

        private static void HandleTEData(List<HtmlNode> playerRows, int week, int year)
        {
            foreach (var row in playerRows)
            {
                var values = row.Descendants("td").ToArray();
                var trimmedValues = new List<string>();
                foreach (var val in values)
                    trimmedValues.Add(Regex.Replace(val.InnerText, @"<[^>]+>|&nbsp;", "").Trim());
                var playerName = NormalizeName(trimmedValues[0]);
                var team = trimmedValues[1].ToLower();
                var targets = trimmedValues[3];
                var receptions = trimmedValues[4];
                var recYards = trimmedValues[5];
                var recTds = trimmedValues[6];
                var fantasyPoints = trimmedValues[7];
                Nullable<int> playerId = null;

                // Attempt to find a player object with the fetched name. If it exists, use that player id.
                // If it doesnt, create the new player and use its new id
                using (SqlConnection conn = new SqlConnection(CONNECTION_STRING))
                {
                    string queryString = "SELECT id FROM  [dbo].[players] WHERE name = @playerName AND position = @position";
                    SqlCommand command = new SqlCommand(queryString, conn);
                    command.Parameters.AddWithValue("@playerName", playerName);
                    command.Parameters.AddWithValue("@position", ((int)PositionEnum.qb).ToString());
                    conn.Open();
                    SqlDataReader reader = command.ExecuteReader();
                    try
                    {
                        if (reader.HasRows)
                        {
                            while (reader.Read())
                            {
                                playerId = (int)reader["id"];
                            }
                        }
                        else
                        {
                            // There are no player names with the matching name, insert a new row.
                            string insertPlayerQuery = "INSERT INTO [dbo].[players](name, position) output INSERTED.ID VALUES (@playerName, @position)";
                            SqlCommand insertPlayerCommand = new SqlCommand(insertPlayerQuery, conn);
                            insertPlayerCommand.Parameters.AddWithValue("@playerName", playerName);
                            insertPlayerCommand.Parameters.AddWithValue("@position", ((int)PositionEnum.te).ToString());
                            var result = insertPlayerCommand.ExecuteScalar();
                            playerId = (int)result;
                        }
                    }
                    catch (Exception ex)
                    {
                        var x = ex;
                    }
                    finally
                    {
                        // Always call Close when done reading.
                        reader.Close();
                    }

                    if (playerId.HasValue)
                    {
                        string insertPerformanceQuery =
                            "INSERT INTO [dbo].[player_performances](player_id, score_value, nfl_week, year, team, " +
                            "targets, receptions, receiving_yards, receiving_tds) " +
                            " VALUES (@playerId, @score, @week, @year, @team, @targets, @receptions, @receiving_yards, @receiving_tds)";
                        SqlCommand insertPerformanceCommand = new SqlCommand(insertPerformanceQuery, conn);
                        insertPerformanceCommand.Parameters.AddWithValue("@playerId", playerId.Value);
                        insertPerformanceCommand.Parameters.AddWithValue("@score", fantasyPoints);
                        insertPerformanceCommand.Parameters.AddWithValue("@week", week);
                        insertPerformanceCommand.Parameters.AddWithValue("@year", year);
                        insertPerformanceCommand.Parameters.AddWithValue("@team", team);
                        insertPerformanceCommand.Parameters.AddWithValue("@targets", targets);
                        insertPerformanceCommand.Parameters.AddWithValue("@receptions", receptions);
                        insertPerformanceCommand.Parameters.AddWithValue("@receiving_yards", recYards);
                        insertPerformanceCommand.Parameters.AddWithValue("@receiving_tds", recTds);
                        var rowInserted = insertPerformanceCommand.ExecuteNonQuery() > 0;
                    }
                    else
                    {
                        // If no playerId is present something went wrong.
                        throw new Exception("No Player ID for TE");
                    }

                    conn.Close();
                }
            }
        }

        private static void HandleDefData(List<HtmlNode> playerRows)
        {
            return;
        }

        public static async Task<IEnumerable<PlayerInfo>> FetchPlayerInfoData()
        {
            HttpClient client = new HttpClient();
            var response = await client.GetAsync(string.Format(MY_FANTASY_BASE_ADDRESS, 2016, PLAYER_REPORT, MY_FANTASY_LEAGUE_ID, ""));
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
                var response = await client.GetAsync(string.Format(MY_FANTASY_BASE_ADDRESS, 2016, INJURY_REPORT, MY_FANTASY_LEAGUE_ID, i));
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
                var response = await client.GetAsync(string.Format(MY_FANTASY_BASE_ADDRESS, 2016, PERFORMANCE_REPORT, MY_FANTASY_LEAGUE_ID, i));
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

            using (FileStream fs = File.Open("playerResults.csv", FileMode.Create))
            {
                using (var writer = new StreamWriter(fs))
                {
                    foreach (var result in results)
                    {
                        var score = result.Value.Scores.Count > 0 ? result.Value.Scores.Last() : 0;
                        var output = string.Format("{0},{1},{2}",
                            result.Value.Name,
                            result.Value.Position,
                            score);
                        writer.WriteLine(output);
                    }
                }
            }
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
            name = name.Trim();
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

        private static PositionEnum TranslateToPositionEnum(string position)
        {
            switch (position)
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
