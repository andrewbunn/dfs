using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace PlayerDataAggregator
{
    public class PlayerResult
    {
        public PlayerResult()
        {
            Scores = new List<float>();
            Injuries = new List<bool>();
        }

        public int Id { get; set; }
        public string Name { get; set; }
        public string Position { get; set; }
        public List<bool> Injuries { get; set; }
        public List<float> Scores { get; set; }
    }
}
