var players = document.getElementsByClassName("dataRow");
var str = "";
for (var i =0; i < players.length; i++)
{
	var arr = players[i].children[0].innerText.split("(");
	var name = arr[0].trim();
	var pos = arr[1].split(" ")[0];
	var proj = players[i].getElementsByClassName("fantasyPPRHalf")[0].children[2].innerText;
	str += name;
	str += ",";
	if (pos == "RB")
	{
		str += "1,"
	}
	if (pos == "WR")
	{
		str += "2,"
	}
	if (pos == "TE")
	{
		str += "3,"
	}
	if (pos == "QB")
	{
		str += "0,"
	}
	str += proj;
	str += "\n";
}

window.location.href = 'data:text/csv;charset=utf-8,' + encodeURIComponent(str);