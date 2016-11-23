var list = document.getElementsByClassName("infinite-scroll-list");
var players = list[0].getElementsByClassName("Cur-p");
var str = "";
for (var i = 0; i < players.length; i++)
{
	var name = players[i].getElementsByClassName("Td-n")[0].innerText;
	name.trim();
	str += name + ",";
	var tds = players[i].getElementsByTagName("td");
	str += tds[5].innerText.slice(1);
	var position = tds[0].innerText;
	var pos;
	switch(position) {
	case "QB":
		pos = 0;
		break;
	case "RB":
		pos = 1;
		break;
	case "WR":
		pos = 2;
		break;
	case "TE":
		pos = 3;
		break;
	case "DEF":
	default:
		pos = 4;
		break;
	}

	str += "," + pos;
	str += "\n";
}
window.location.href = 'data:text/csv;charset=utf-8,' + encodeURIComponent(str);
