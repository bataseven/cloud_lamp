function sendLEDMode(command) {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "/command?value=" + command, true);
    xhr.send();
}

var colorPicker;

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
        
        // Set the initial background color of the page
        document.body.style.backgroundColor = colorPicker.color.hexString;

        // Event listener for color changes
        colorPicker.on('color:change', function (color) {
            // Update the background color of the page with the selected color
            document.body.style.backgroundColor = color.hexString;

            // Get the RGB values of the selected color
            let rgb = color.rgb;

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
        
        // Set the initial background color of the page
        document.body.style.backgroundColor = colorPicker.color.hexString;

        // Event listener for color changes
        colorPicker.on('color:change', function (color) {
            // Update the background color of the page with the selected color
            document.body.style.backgroundColor = color.hexString;

            // Get the RGB values of the selected color
            let rgb = color.rgb;

            // Send the request every 50ms to prevent the server from being overloaded
            if (this.timeout) clearTimeout(this.timeout);
            this.timeout = setTimeout(() => {
                var xhr = new XMLHttpRequest();
                xhr.open("GET", `/color?r=${rgb.r}&g=${rgb.g}&b=${rgb.b}`, true);
                xhr.send();
            }, 50);
        });
    });
