using HtmlAgilityPack;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;

namespace GeneratePlayerData
{
    class Program
    {
        private const string YAHOO_PLAYER_DATA_URI = "https://dfyql-ro.sports.yahoo.com/v2/external/playersFeed/nfl";
        private const string PROJECTIONS_DATA_URI = "http://www.numberfire.com/nfl/daily-fantasy/daily-football-projections";

        static void Main(string[] args)
        {
            var yahooPlayerData = FetchYahooPlayerData().Result;
            var projectionsData = FetchNumberfireProjectionsData().Result;
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
            return null;
        }
    }
}
