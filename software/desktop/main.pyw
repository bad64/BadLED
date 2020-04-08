# Qt
from PyQt5.QtWidgets import QApplication, QWidget
from PyQt5.QtWidgets import QVBoxLayout, QHBoxLayout, QFrame
from PyQt5.QtWidgets import QColorDialog, QComboBox, QLabel, QPushButton
from PyQt5.QtWidgets import QStyleFactory
from PyQt5.QtGui import QColor, qRgb

# Serial
import serial, serial.tools.list_ports

# Other
import glob, sys

class arcadeButton(QWidget):
    def __init__(self, name, parent, colorinfo):
        super(arcadeButton, self).__init__(parent)
        # Properties
        self.name = QLabel(name)
        self.parent = parent
        self.passiveColor = QColor(qRgb(colorinfo[0], colorinfo[1], colorinfo[2]))
        self.activeColor = QColor(qRgb(colorinfo[3], colorinfo[4], colorinfo[5]))

        # Content
        self.passiveColorButton = QPushButton("Passive")
        self.passiveColorButton.clicked.connect(lambda: self.openColorDialog(0))
        self.passiveColorText = QLabel("({}, {}, {})".format(self.passiveColor.red(), self.passiveColor.green(), self.passiveColor.blue()))

        self.activeColorButton = QPushButton("Active")
        self.activeColorButton.clicked.connect(lambda: self.openColorDialog(1))
        self.activeColorText = QLabel("({}, {}, {})".format(self.activeColor.red(), self.activeColor.green(), self.activeColor.blue()))

        # Layout
        self.mainBox = QVBoxLayout()
        self.subBox0 = QHBoxLayout()
        self.subBox1 = QHBoxLayout()
        self.subBox2 = QHBoxLayout()

        self.subBox0.addWidget(self.name)
        
        self.subBox1.addWidget(self.passiveColorButton)
        self.subBox1.addWidget(self.passiveColorText)

        self.subBox2.addWidget(self.activeColorButton)
        self.subBox2.addWidget(self.activeColorText)

        self.mainBox.addLayout(self.subBox0)
        self.mainBox.addLayout(self.subBox1)
        self.mainBox.addLayout(self.subBox2)

        # Display
        self.setLayout(self.mainBox)

    # Functions
    def openColorDialog(self, selector):
        """Opens a color picker and updates the relevant button data"""
        color = QColorDialog.getColor()
        if color.isValid():
            if selector == 0:
                self.passiveColor = color
                self.passiveColorText.setText("({}, {}, {})".format(self.passiveColor.red(), self.passiveColor.green(), self.passiveColor.blue()))
            elif selector == 1:
                self.activeColor = color
                self.activeColorText.setText("({}, {}, {})".format(self.activeColor.red(), self.activeColor.green(), self.activeColor.blue()))

class MainWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("BadLED Control Deck")

        # Ports
        self.portsLabel = QLabel("Ports: ")
        self.serial = serial.Serial()
        self.ports = self.getPorts()
        self.portsCombo = QComboBox()
        self.buildPortsComboBox(self.ports)
        self.upload = QPushButton("Upload")
        self.upload.clicked.connect(lambda: self.uploadValues())

        # Buttons
        self.names = [ "RIGHT", "DOWN", "LEFT", "UP", "1P", "2P", "3P", "4P", "1K", "2K", "3K", "4K" ]
        self.numberOfButtons = int(self.getButtons());
        self.buttons = self.buildButtonArray()

        # Panels
        self.panels = []
        self.panelLayouts = []

        for i in range(int(self.numberOfButtons / 4)):
            self.panels.append(QFrame(self))
            self.panelLayouts.append(QVBoxLayout())

        for i in range(len(self.panelLayouts)):
            for j in range(i*4, i*4+4):
                self.panelLayouts[i].addWidget(self.buttons[j])
            self.panels[i].setLayout(self.panelLayouts[i])
            self.panels[i].setFrameStyle(QFrame.Raised | QFrame.Panel)
            
        # Layout
        self.mainVBox = QVBoxLayout()       # Main layout
        self.portsHBox = QHBoxLayout()      # Holds the port widgets
        self.buttonsHBox = QHBoxLayout()    # Holds the panels that contain the arcadeButton widgets

        self.portsHBox.addWidget(self.portsLabel)
        self.portsHBox.addWidget(self.portsCombo)
        self.portsHBox.addWidget(self.upload)

        for i in range(len(self.panels)):
            self.buttonsHBox.addWidget(self.panels[i])

        self.mainVBox.addLayout(self.portsHBox)
        self.mainVBox.addLayout(self.buttonsHBox)

        self.setLayout(self.mainVBox)

        # Show
        self.show()
        
    def uploadValues(self):
        values = "set allbuttons "

        for item in self.buttons:
            values += str(item.passiveColor.red())
            values += " "
            values += str(item.passiveColor.green())
            values += " "
            values += str(item.passiveColor.blue())
            values += " "
            values += str(item.activeColor.red())
            values += " "
            values += str(item.activeColor.green())
            values += " "
            values += str(item.activeColor.blue())
            values += " "
        values = bytearray(values, 'utf-8')
            
        s = serial.Serial(self.portsCombo.currentText(), timeout=5)
        resp = s.readline()
        s.write(values)
        resp = s.readline()
        if "Received:" in resp.decode():
            resp = s.readline()
        s.close()

    def getPorts(self):
        """Gets a list of available serial ports"""
        if sys.platform.startswith('win'):
            ports = ['COM%s' % (i + 1) for i in range(256)]
        elif sys.platform.startswith('linux'):
            ports = glob.glob('/dev/tty[A-Za-z]*')
        elif sys.platform.startswith('darwin'):
            ports = glob.glob('/dev/tty.*')
        else:
            
            raise EnvironmentError('Unsupported platform')

        result = []
        for port in ports:
            try:
                s = serial.Serial(port)
                s.close()
                result.append(port)
            except (OSError, serial.SerialException):
                pass
        return result

    def buildPortsComboBox(self, arr):
        """Creates a combo box containing the list of available serial ports."""
        for i in range(len(arr)):
            self.portsCombo.addItem(arr[i])

    def getButtons(self):
        """Gets the number of buttons on the stick. Implicitly trusts the hardware to give the right info."""
        s = serial.Serial(self.portsCombo.currentText(), timeout=5)
        resp = s.readline()
        s.write("hwinfo".encode())
        resp = s.readline()
        if "Received:" in resp.decode():
            resp = s.readline()
        s.close()
        return resp.decode()

    def getColors(self):
        """Gets color data from the Arduino."""
        s = serial.Serial(self.portsCombo.currentText(), timeout=5)
        resp = s.readline()
        s.write("getcolorinfo".encode())
        resp = s.readline()
        if "Received:" in resp.decode():
            resp = s.readline()
        s.close()
        return resp

    def buildButtonArray(self):
        """Populates the array of arcadeButton widgets using color information provided by the hardware"""
        arr = []
        colorinfo = self.getColors()
        
        for i in range(self.numberOfButtons - 1):
            j = i * 6
            arr.append(arcadeButton(self.names[i], self, colorinfo[j:j+6]))
        return arr


if __name__ == '__main__':
    app = QApplication(sys.argv)
    context = MainWindow()
    sys.exit(app.exec_())
