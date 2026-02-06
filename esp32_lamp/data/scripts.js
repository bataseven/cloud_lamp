function sendLEDMode(command) {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "/command?value=" + command, true);
    xhr.send();
}

var colorPicker;
var alarm = {
    enabled: false,
    time: "",
    brightness: 100,
    lastTriggeredDate: "",
    rampStartedDate: "",
    rampInProgress: false
};
var alarmSaveTimer = null;
var isUserInteracting = false;
var isRemoteUpdate = false;
var lastRemoteBrightness = null;
var lastRemoteColor = null;
var currentBrightness = 100;
var isAlarmEditing = false;

function loadAlarmState() {
    fetch('/getAlarm')
        .then(response => response.json())
        .then(saved => {
            if (typeof saved.enabled === "boolean") alarm.enabled = saved.enabled;
            if (typeof saved.time === "string") alarm.time = saved.time;
            applyAlarmStateToUI();
        })
        .catch(error => {
            console.warn("Failed to load alarm state", error);
            applyAlarmStateToUI();
        });
}

function fetchAlarmState() {
    if (isAlarmEditing) return;
    fetch('/getAlarm')
        .then(response => response.json())
        .then(saved => {
            var changed = false;
            if (typeof saved.enabled === "boolean" && saved.enabled !== alarm.enabled) {
                alarm.enabled = saved.enabled;
                changed = true;
            }
            if (typeof saved.time === "string" && saved.time !== alarm.time) {
                alarm.time = saved.time;
                changed = true;
            }
            if (changed) applyAlarmStateToUI();
        })
        .catch(error => {
            console.warn("Failed to fetch alarm state", error);
        });
}

function saveAlarmState() {
    if (alarmSaveTimer) clearTimeout(alarmSaveTimer);
    alarmSaveTimer = setTimeout(() => {
        var params = new URLSearchParams({
            enabled: alarm.enabled ? "1" : "0",
            time: alarm.time || ""
        });
        fetch('/setAlarm?' + params.toString())
            .catch(error => console.warn("Failed to save alarm state", error));
    }, 200);
}

function applyAlarmStateToUI() {
    var alarmTimeInput = document.getElementById("alarmTime");
    var alarmEnabledInput = document.getElementById("alarmEnabled");

    if (alarmTimeInput) {
        alarmTimeInput.value = alarm.time || "";
    }
    if (alarmEnabledInput) {
        alarmEnabledInput.checked = alarm.enabled;
    }
    updateAlarmStatus(alarm.enabled && alarm.time ? "Alarm: set for " + alarm.time : "Alarm: off");
}

function sendBrightness(value) {
    currentBrightness = value;
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "/brightness?value=" + value, true);
    xhr.send();
}

function rampBrightness(target, durationMs, onComplete) {
    var tickMs = 1000;
    var steps = Math.max(1, Math.floor(durationMs / tickMs));
    var current = currentBrightness;
    if (current >= target) {
        sendBrightness(target);
        if (onComplete) onComplete();
        return;
    }
    var delta = target - current;
    var step = Math.max(1, Math.round(delta / steps));

    var interval = setInterval(() => {
        current += step;
        if (current >= target) {
            current = target;
        }
        sendBrightness(current);
        if (current >= target) {
            clearInterval(interval);
            if (onComplete) onComplete();
        }
    }, tickMs);
}

function rampBrightnessFast(target) {
    var durationMs = 10 * 1000;
    var tickMs = 200;
    var steps = Math.max(1, Math.floor(durationMs / tickMs));
    var current = 0;
    var step = Math.max(1, Math.round(target / steps));

    var interval = setInterval(() => {
        current += step;
        if (current >= target) {
            current = target;
        }
        sendBrightness(current);
        if (current >= target) {
            clearInterval(interval);
        }
    }, tickMs);
}

function updateAlarmStatus(text) {
    var el = document.getElementById("alarmStatus");
    if (el) el.textContent = text;
}

function applyRemoteBrightness(value) {
    if (!colorPicker || isUserInteracting) return;
    if (lastRemoteBrightness !== null && Math.abs(value - lastRemoteBrightness) < 1) return;
    var hsv = colorPicker.color.hsv;
    isRemoteUpdate = true;
    colorPicker.color.set({ h: hsv.h, s: hsv.s, v: value });
    isRemoteUpdate = false;
    lastRemoteBrightness = value;
    currentBrightness = value;
    document.body.style.backgroundColor = colorPicker.color.hexString;
}

function fetchBrightness() {
    fetch('/getBrightness')
        .then(response => response.text())
        .then(text => {
            var value = parseInt(text, 10);
            if (Number.isNaN(value)) return;
            applyRemoteBrightness(value);
        })
        .catch(error => {
            console.warn('Error fetching brightness:', error);
        });
}

function applyRemoteColor(color) {
    if (!colorPicker || isUserInteracting) return;
    if (lastRemoteColor && lastRemoteColor.toLowerCase() === color.toLowerCase()) return;
    isRemoteUpdate = true;
    colorPicker.color.set(color);
    isRemoteUpdate = false;
    lastRemoteColor = color;
    currentBrightness = colorPicker.color.hsv.v;
    document.body.style.backgroundColor = colorPicker.color.hexString;
}

function fetchColor() {
    fetch('/getColor')
        .then(response => response.text())
        .then(color => {
            if (!/^#[0-9A-F]{6}$/i.test(color)) return;
            applyRemoteColor(color);
        })
        .catch(error => {
            console.warn('Error fetching color:', error);
        });
}

function getLocalDateString() {
    var now = new Date();
    return now.getFullYear() + "-" +
        String(now.getMonth() + 1).padStart(2, "0") + "-" +
        String(now.getDate()).padStart(2, "0");
}

function checkAlarm() {
    if (!alarm.enabled || !alarm.time) return;

    var now = new Date();
    var today = getLocalDateString();
    var hh = String(now.getHours()).padStart(2, "0");
    var mm = String(now.getMinutes()).padStart(2, "0");
    var currentTime = hh + ":" + mm;

    var parts = alarm.time.split(":");
    if (parts.length !== 2) return;
    var alarmHour = parseInt(parts[0], 10);
    var alarmMinute = parseInt(parts[1], 10);
    if (Number.isNaN(alarmHour) || Number.isNaN(alarmMinute)) return;

    var alarmDate = new Date(now.getFullYear(), now.getMonth(), now.getDate(), alarmHour, alarmMinute, 0, 0);
    var diffMs = alarmDate.getTime() - now.getTime();
    var fiveMinMs = 5 * 60 * 1000;

    if (diffMs > 0 && diffMs <= fiveMinMs && alarm.rampStartedDate !== today && !alarm.rampInProgress) {
        alarm.rampStartedDate = today;
        alarm.rampInProgress = true;
        updateAlarmStatus("Alarm: ramping to " + alarm.time);
        rampBrightness(alarm.brightness, diffMs, function () {
            alarm.rampInProgress = false;
            alarm.enabled = false;
            saveAlarmState();
            updateAlarmStatus("Alarm: completed");
        });
        return;
    }

    if (currentTime === alarm.time && alarm.lastTriggeredDate !== today && !alarm.rampInProgress) {
        alarm.lastTriggeredDate = today;
        alarm.enabled = false;
        saveAlarmState();
        updateAlarmStatus("Alarm: triggered at " + currentTime);
        alarm.rampInProgress = true;
        rampBrightness(alarm.brightness, fiveMinMs, function () {
            alarm.rampInProgress = false;
            alarm.enabled = false;
            saveAlarmState();
            updateAlarmStatus("Alarm: completed");
        });
    }
}

// Fetch the saved color from the server (or set a default color)
fetch('/getColor')
    .then(response => response.text())
    .then(color => {
        // Validate the color (as shown before)
        if (!/^#[0-9A-F]{6}$/i.test(color)) {
            console.warn('Invalid color string received:', color);
            color = '#ff0000'; // Default color if invalid
        }

        // Initialize the color picker
        colorPicker = new iro.ColorPicker("#picker", {
            width: 320,
            color: color  // Set the initial color
        });

        colorPicker.on('input:start', function () {
            isUserInteracting = true;
        });
        colorPicker.on('input:end', function () {
            isUserInteracting = false;
        });
        
        // Set the initial background color of the page
        document.body.style.backgroundColor = colorPicker.color.hexString;

        // Event listener for color changes
        colorPicker.on('color:change', function (color) {
            if (isRemoteUpdate) return;
            // Update the background color of the page with the selected color
            document.body.style.backgroundColor = color.hexString;

            // Get the RGB values of the selected color
            let rgb = color.rgb;
            currentBrightness = color.hsv.v;

            // Send the request every 50ms to prevent the server from being overloaded
            if (this.timeout) clearTimeout(this.timeout);
            this.timeout = setTimeout(() => {
                var xhr = new XMLHttpRequest();
                xhr.open("GET", `/color?r=${rgb.r}&g=${rgb.g}&b=${rgb.b}`, true);
                xhr.send();
            }, 50);
        });
    })
    .catch(error => {
        console.error('Error fetching color:', error);
        
        // Initialize with default color in case of error
        colorPicker = new iro.ColorPicker("#picker", {
            width: 320,
            color: '#ff0000'  // Default color
        });

        colorPicker.on('input:start', function () {
            isUserInteracting = true;
        });
        colorPicker.on('input:end', function () {
            isUserInteracting = false;
        });
        
        // Set the initial background color of the page
        document.body.style.backgroundColor = colorPicker.color.hexString;

        // Event listener for color changes
        colorPicker.on('color:change', function (color) {
            if (isRemoteUpdate) return;
            // Update the background color of the page with the selected color
            document.body.style.backgroundColor = color.hexString;

            // Get the RGB values of the selected color
            let rgb = color.rgb;
            currentBrightness = color.hsv.v;

            // Send the request every 50ms to prevent the server from being overloaded
            if (this.timeout) clearTimeout(this.timeout);
            this.timeout = setTimeout(() => {
                var xhr = new XMLHttpRequest();
                xhr.open("GET", `/color?r=${rgb.r}&g=${rgb.g}&b=${rgb.b}`, true);
                xhr.send();
            }, 50);
        });
    });

// Alarm UI wiring
var alarmTimeInput = document.getElementById("alarmTime");
var alarmEnabledInput = document.getElementById("alarmEnabled");
var alarmTestBtn = document.getElementById("alarmTest");

loadAlarmState();

if (alarmTimeInput) {
    alarmTimeInput.addEventListener("change", function () {
        alarm.time = alarmTimeInput.value;
        saveAlarmState();
        updateAlarmStatus(alarm.enabled ? "Alarm: set for " + alarm.time : "Alarm: off");
    });
    alarmTimeInput.addEventListener("focus", function () {
        isAlarmEditing = true;
    });
    alarmTimeInput.addEventListener("blur", function () {
        isAlarmEditing = false;
    });
}

if (alarmEnabledInput) {
    alarmEnabledInput.addEventListener("change", function () {
        alarm.enabled = alarmEnabledInput.checked;
        saveAlarmState();
        updateAlarmStatus(alarm.enabled && alarm.time ? "Alarm: set for " + alarm.time : "Alarm: off");
    });
    alarmEnabledInput.addEventListener("focus", function () {
        isAlarmEditing = true;
    });
    alarmEnabledInput.addEventListener("blur", function () {
        isAlarmEditing = false;
    });
}

if (alarmTestBtn) {
    alarmTestBtn.addEventListener("click", function () {
        updateAlarmStatus("Alarm: test triggered");
        rampBrightnessFast(alarm.brightness);
    });
}

setInterval(checkAlarm, 1000);
setInterval(fetchBrightness, 1000);
setInterval(fetchColor, 1000);
setInterval(fetchAlarmState, 1000);
