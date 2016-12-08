using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace PlayerDataAggregator
{
    public class FFAnalyticsProjectionsRequest
    {
        public const string SCORING_RULES = "\"scoringRules\":[{\"positionId\":\"1\",\"positionCode\":\"QB\",\"dataCol\":\"passYds\",\"multiplier\":0.04},{\"positionId\":\"1\",\"positionCode\":\"QB\",\"dataCol\":\"passAtt\"},{\"positionId\":\"1\",\"positionCode\":\"QB\",\"dataCol\":\"passComp\"},{\"positionId\":\"1\",\"positionCode\":\"QB\",\"dataCol\":\"passInc\"},{\"positionId\":\"1\",\"positionCode\":\"QB\",\"dataCol\":\"passTds\",\"multiplier\":\"4\"},{\"positionId\":\"1\",\"positionCode\":\"QB\",\"dataCol\":\"passInt\",\"multiplier\":\"-1\"},{\"positionId\":\"1\",\"positionCode\":\"QB\",\"dataCol\":\"pass40\"},{\"positionId\":\"1\",\"positionCode\":\"QB\",\"dataCol\":\"pass300\"},{\"positionId\":\"1\",\"positionCode\":\"QB\",\"dataCol\":\"pass350\"},{\"positionId\":\"1\",\"positionCode\":\"QB\",\"dataCol\":\"pass400\"},{\"positionId\":\"1\",\"positionCode\":\"QB\",\"dataCol\":\"rushYds\",\"multiplier\":0.1},{\"positionId\":\"1\",\"positionCode\":\"QB\",\"dataCol\":\"sacks\"},{\"positionId\":\"1\",\"positionCode\":\"QB\",\"dataCol\":\"rushAtt\"},{\"positionId\":\"1\",\"positionCode\":\"QB\",\"dataCol\":\"rush40\"},{\"positionId\":\"1\",\"positionCode\":\"QB\",\"dataCol\":\"rushTds\",\"multiplier\":\"6\"},{\"positionId\":\"1\",\"positionCode\":\"QB\",\"dataCol\":\"twoPts\",\"multiplier\":\"2\"},{\"positionId\":\"1\",\"positionCode\":\"QB\",\"dataCol\":\"fumbles\",\"multiplier\":\"-2\"},{\"positionId\":\"2\",\"positionCode\":\"RB\",\"dataCol\":\"rushYds\",\"multiplier\":0.1},{\"positionId\":\"2\",\"positionCode\":\"RB\",\"dataCol\":\"rushAtt\"},{\"positionId\":\"2\",\"positionCode\":\"RB\",\"dataCol\":\"rushTds\",\"multiplier\":\"6\"},{\"positionId\":\"2\",\"positionCode\":\"RB\",\"dataCol\":\"rush40\"},{\"positionId\":\"2\",\"positionCode\":\"RB\",\"dataCol\":\"rush100\"},{\"positionId\":\"2\",\"positionCode\":\"RB\",\"dataCol\":\"rush150\"},{\"positionId\":\"2\",\"positionCode\":\"RB\",\"dataCol\":\"rush200\"},{\"positionId\":\"2\",\"positionCode\":\"RB\",\"dataCol\":\"rec\",\"multiplier\":\"0.5\"},{\"positionId\":\"2\",\"positionCode\":\"RB\",\"dataCol\":\"recYds\",\"multiplier\":0.1},{\"positionId\":\"2\",\"positionCode\":\"RB\",\"dataCol\":\"recTds\",\"multiplier\":\"6\"},{\"positionId\":\"2\",\"positionCode\":\"RB\",\"dataCol\":\"rec40\"},{\"positionId\":\"2\",\"positionCode\":\"RB\",\"dataCol\":\"returnYds\",\"multiplier\":null},{\"positionId\":\"2\",\"positionCode\":\"RB\",\"dataCol\":\"returnTds\",\"multiplier\":\"6\"},{\"positionId\":\"2\",\"positionCode\":\"RB\",\"dataCol\":\"twoPts\",\"multiplier\":\"2\"},{\"positionId\":\"2\",\"positionCode\":\"RB\",\"dataCol\":\"fumbles\",\"multiplier\":\"-2\"},{\"positionId\":\"3\",\"positionCode\":\"WR\",\"dataCol\":\"rushYds\",\"multiplier\":0.1},{\"positionId\":\"3\",\"positionCode\":\"WR\",\"dataCol\":\"rushAtt\"},{\"positionId\":\"3\",\"positionCode\":\"WR\",\"dataCol\":\"rushTds\",\"multiplier\":\"6\"},{\"positionId\":\"3\",\"positionCode\":\"WR\",\"dataCol\":\"rush40\"},{\"positionId\":\"3\",\"positionCode\":\"WR\",\"dataCol\":\"rec\",\"multiplier\":\"0.5\"},{\"positionId\":\"3\",\"positionCode\":\"WR\",\"dataCol\":\"recYds\",\"multiplier\":0.1},{\"positionId\":\"3\",\"positionCode\":\"WR\",\"dataCol\":\"recTds\",\"multiplier\":\"6\"},{\"positionId\":\"3\",\"positionCode\":\"WR\",\"dataCol\":\"rec40\"},{\"positionId\":\"3\",\"positionCode\":\"WR\",\"dataCol\":\"rec100\"},{\"positionId\":\"3\",\"positionCode\":\"WR\",\"dataCol\":\"rec150\"},{\"positionId\":\"3\",\"positionCode\":\"WR\",\"dataCol\":\"rec200\"},{\"positionId\":\"3\",\"positionCode\":\"WR\",\"dataCol\":\"returnYds\",\"multiplier\":null},{\"positionId\":\"3\",\"positionCode\":\"WR\",\"dataCol\":\"returnTds\",\"multiplier\":\"6\"},{\"positionId\":\"3\",\"positionCode\":\"WR\",\"dataCol\":\"twoPts\",\"multiplier\":\"2\"},{\"positionId\":\"3\",\"positionCode\":\"WR\",\"dataCol\":\"fumbles\",\"multiplier\":\"-2\"},{\"positionId\":\"4\",\"positionCode\":\"TE\",\"dataCol\":\"rushYds\",\"multiplier\":0.1},{\"positionId\":\"4\",\"positionCode\":\"TE\",\"dataCol\":\"rushAtt\"},{\"positionId\":\"4\",\"positionCode\":\"TE\",\"dataCol\":\"rushTds\",\"multiplier\":\"6\"},{\"positionId\":\"4\",\"positionCode\":\"TE\",\"dataCol\":\"rush40\"},{\"positionId\":\"4\",\"positionCode\":\"TE\",\"dataCol\":\"rec\",\"multiplier\":\"0.5\"},{\"positionId\":\"4\",\"positionCode\":\"TE\",\"dataCol\":\"recYds\",\"multiplier\":0.1},{\"positionId\":\"4\",\"positionCode\":\"TE\",\"dataCol\":\"recTds\",\"multiplier\":\"6\"},{\"positionId\":\"4\",\"positionCode\":\"TE\",\"dataCol\":\"rec40\"},{\"positionId\":\"4\",\"positionCode\":\"TE\",\"dataCol\":\"rec100\"},{\"positionId\":\"4\",\"positionCode\":\"TE\",\"dataCol\":\"rec150\"},{\"positionId\":\"4\",\"positionCode\":\"TE\",\"dataCol\":\"rec200\"},{\"positionId\":\"4\",\"positionCode\":\"TE\",\"dataCol\":\"returnYds\",\"multiplier\":null},{\"positionId\":\"4\",\"positionCode\":\"TE\",\"dataCol\":\"returnTds\",\"multiplier\":\"6\"},{\"positionId\":\"4\",\"positionCode\":\"TE\",\"dataCol\":\"twoPts\",\"multiplier\":\"2\"},{\"positionId\":\"4\",\"positionCode\":\"TE\",\"dataCol\":\"fumbles\",\"multiplier\":\"-2\"},{\"positionId\":\"6\",\"positionCode\":\"DST\",\"dataCol\":\"dstFumlRec\",\"multiplier\":\"2\"},{\"positionId\":\"6\",\"positionCode\":\"DST\",\"dataCol\":\"dstInt\",\"multiplier\":\"2\"},{\"positionId\":\"6\",\"positionCode\":\"DST\",\"dataCol\":\"dstSafety\",\"multiplier\":\"2\"},{\"positionId\":\"6\",\"positionCode\":\"DST\",\"dataCol\":\"dstSack\",\"multiplier\":\"1\"},{\"positionId\":\"6\",\"positionCode\":\"DST\",\"dataCol\":\"dstTd\",\"multiplier\":\"6\"},{\"positionId\":\"6\",\"positionCode\":\"DST\",\"dataCol\":\"dstBlk\",\"multiplier\":\"2\"},{\"positionId\":\"6\",\"positionCode\":\"DST\",\"dataCol\":\"returnYds\",\"multiplier\":null},{\"positionId\":\"6\",\"positionCode\":\"DST\",\"dataCol\":\"dstPtsAllow\"}]";

        [JsonProperty("ShouldImpute")]
        public bool ShouldImpute = false;

        [JsonProperty("aavOption")]
        public string AavOption = "AVG";

        [JsonProperty("adpOption")]
        public string AdpOption = "AVG";

        [JsonProperty("analysts")]
        public List<FFAnalyst> Analysts { get; set; }

        [JsonProperty("calctype")]
        public string CalcType = "\"weighted\"";

        [JsonProperty("isInitialCall")]
        public bool IsInitialCall = false;

        [JsonProperty("positionVorBaselines")]
        public List<PositionVorBaseline> PositionVorBaselines = new List<PositionVorBaseline>()
        {
            new PositionVorBaseline("QB", "11"),
            new PositionVorBaseline("RB", "30"),
            new PositionVorBaseline("WR", "37"),
            new PositionVorBaseline("TE", "7"),
            new PositionVorBaseline("K", "0"),
            new PositionVorBaseline("DST", "0"),
            new PositionVorBaseline("DL", "5"),
            new PositionVorBaseline("DB", "1"),
            new PositionVorBaseline("LB", "15")
        };

        [JsonProperty("positions")]
        public List<string> Positions = new List<string>()
        {
            "QB",
            "RB",
            "WR",
            "TE",
            "DST"
        };

        [JsonProperty("ptsBracket")]
        List<PointsBracket> PointsBracket = new List<PointsBracket>()
        {
            new PointsBracket("0", "10"),
            new PointsBracket("6", "7"),
            new PointsBracket("13", "4"),
            new PointsBracket("20", "1"),
            new PointsBracket("27", "0"),
            new PointsBracket("34", "-1"),
            new PointsBracket("99", "-4")
        };

        [JsonProperty("season")]
        public int Season { get; set; }

        [JsonProperty("week")]
        public string Week { get; set; }
    }

    public class FFAnalyst
    {
        [JsonProperty("analystId")]
        public int Id { get; set; }

        [JsonProperty("dataScrapeId")]
        public int DataScrapeId { get; set; }

        [JsonProperty("analystName")]
        public string Name { get; set; }

        [JsonProperty("analystWeight")]
        public string Weight { get; set; }
    }

    public class PositionVorBaseline
    {
        public PositionVorBaseline(string position, string rank)
        {
            this.PositionCode = position;
            this.VorRank = rank;
        }

        [JsonProperty("positionCode")]
        public string PositionCode { get; set; }

        [JsonProperty("vorRank")]
        public string VorRank { get; set; }
    }

    public class PointsBracket
    {
        public PointsBracket(string threshold, string points)
        {
            this.Threshold = threshold;
            this.Points = points;
        }

        [JsonProperty("ptsBracketId")]
        public int PtsBracketId = 0;

        [JsonProperty("leagueId")]
        public int LeagueId = 0;

        [JsonProperty("threshold")]
        public string Threshold { get; set; }

        [JsonProperty("points")]
        public string Points { get; set; }
    }
}
