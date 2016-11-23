var players = document.getElementsByClassName("team-player");
var projs = document.getElementsByClassName("fp");
var str = "";
for (var i = 0; i < players.length; i++)
{
	var name = players[i].getElementsByClassName("full")[0].innerText;
	name = name.slice(0, name.indexOf("("));
	name.trim();
	str += name + "," + projs[i].innerText;
	str += "\n";
}

window.location.href = 'data:text/csv;charset=utf-8,' + encodeURIComponent(str);
