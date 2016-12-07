var players = document.getElementsByClassName("dataRow");
var str = "";
for (var i =0; i < players.length; i++)
{
	var name = players[i].children[0].innerText.split("(")[0].trim();
	var proj = players[i].getElementsByClassName("fantasyPPRHalf")[0].children[2].innerText;
	str += name;
	str += ",";
	str += proj;
	str += "\n";
}

window.location.href = 'data:text/csv;charset=utf-8,' + encodeURIComponent(str);