console.log("top");
// IP of the robot
var IProbot = "192.168.1.115:8080";

function getLed(IP){
    fetch("http://" + IProbot + "/led", {
        method: 'GET',
        headers: { "Content-Type": "application/json;charset=UTF-8"}
    })
    .then(res => res.json())
    .then(res =>document.getElementById("label").innerHTML=res.value);
}

document.getElementById("BPgetLED").addEventListener("click", function (evt) {
    getLed(IProbot);
});

document.getElementById("BPsetLEDon").addEventListener("click", function (evt) {
    fetch("http://" + IProbot + "/setled", {
        mode: 'no-cors',
        method: 'POST',
        body: '1',
        headers: { "Content-Type": "application/json;charset=UTF-8"}
    })
});

document.getElementById("BPsetLEDoff").addEventListener("click", function (evt) {
    fetch("http://" + IProbot + "/setled", {
        mode: 'no-cors',
        method: 'POST',
        body: '0',
        headers: { "Content-Type": "application/json;charset=UTF-8"}
    })
});

document.getElementById("bar").addEventListener("input", function () {
    fetch("http://" + IProbot + "/setservo", {
        mode: 'no-cors',
        method: 'POST',
        body: document.getElementById("bar").value,
        headers: { "Content-Type": "application/json;charset=UTF-8"}
    })
});

setInterval(function(){
    getLed(IProbot);
},100);