using HtmlAgilityPack;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace GeneratePlayerData
{
    public class NumberFireZippedPlayerInfo
    {
        public NumberFireZippedPlayerInfo()
        {
            Info = new List<HtmlNode>();
            Projections = new List<HtmlNode>();
            Position = new List<PositionEnum>();
        }

        public List<HtmlNode> Info { get; set; }
        public List<HtmlNode> Projections { get; set; }
        public List<PositionEnum> Position { get; set; }
    }
}
