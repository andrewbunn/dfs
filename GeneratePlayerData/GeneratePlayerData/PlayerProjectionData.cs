using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace GeneratePlayerData
{
    public class PlayerProjectionData
    {
        public string Name { get; set; }
        public decimal Projection { get; set; }
        public string Position { get; set; }
        public decimal StandardDeviation { get; set; }

        public PositionEnum PositionEnum
        {
            get
            {
                return (PositionEnum)Enum.Parse(typeof(PositionEnum), this.Position.ToLower());
            }
        }
    }
}
