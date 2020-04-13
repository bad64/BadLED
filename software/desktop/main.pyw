# Qt
from PyQt5.QtWidgets import QApplication, QMessageBox, QWidget
from PyQt5.QtWidgets import QVBoxLayout, QHBoxLayout, QGridLayout, QFrame
from PyQt5.QtWidgets import QColorDialog, QComboBox, QLabel, QPushButton, QSpinBox
from PyQt5.QtWidgets import QCheckBox
from PyQt5.QtGui import QIcon, QColor, QPalette, qRgb

# Serial
import serial, serial.tools.list_ports

# Other
import glob, sys

# Flags pseudo defines
FLAGS_USE_LOOPBACK            = 128
FLAGS_JOYSTICK_HAS_LEDS       = 64
FLAGS_JOYSTICK_LEDS_ARE_FIRST = 32
FLAGS_HAS_4P                  = 16
FLAGS_FOUR_P_TURNS_ON_ALL_P   = 8
FLAGS_HAS_4K                  = 4
FLAGS_FOUR_K_TURNS_ON_ALL_K   = 2
FLAGS_HAS_EXTRA_UP_BUTTON     = 1

# Widgets
class delayWidget(QWidget):
    def __init__(self, parent, delay):
        super(delayWidget, self).__init__(parent)
        self.parent = parent

        self.label = QLabel("Glow fade-out speed: ")
        self.spinbox = QSpinBox(self)
        self.spinbox.setMaximum(255)
        self.spinbox.setValue(delay)
        self.button = QPushButton("Update glow fade-out speed")
        self.button.clicked.connect(lambda: self.setDelay())

        self.layout = QHBoxLayout()

        self.layout.addWidget(self.label)
        self.layout.addWidget(self.spinbox)
        self.layout.addWidget(self.button)

        self.setLayout(self.layout)

    def setDelay(self):
        """Sets the delay between two button updates"""
        commandSet(self.parent.portSelect.portsCombo.currentText(), "set delay {}".format(self.spinbox.value()), self.parent.flags.flags)

class portsWidget(QWidget):
    def __init__(self, parent):
        super(portsWidget, self).__init__(parent)
        self.parent = parent

        self.portsLabel = QLabel("Serial port: ")
        self.serial = serial.Serial()
        self.ports = parent.getPorts()
        self.portsCombo = QComboBox()
        self.buildPortsComboBox(self.ports)
        self.upload = QPushButton("Update colors")
        self.upload.clicked.connect(lambda: self.uploadValues())

        self.layout = QHBoxLayout()

        self.layout.addWidget(self.portsLabel)
        self.layout.addWidget(self.portsCombo)
        self.layout.addWidget(self.upload)

        self.setLayout(self.layout)

    def uploadValues(self):
        """Uploads RGB values to the board"""
        values = "set buttons "

        for item in self.parent.buttons:
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
            
        commandSet(self.portsCombo.currentText(), values, self.parent.flags.flags)

    def buildPortsComboBox(self, arr):
        """Creates a combo box containing the list of available serial ports."""
        for i in range(len(arr)):
            self.portsCombo.addItem(arr[i])      

class flagsWidget(QWidget):
    def __init__(self, parent, flags):
        super(flagsWidget, self).__init__(parent)
        self.parent = parent
        self.flags = flags

        # Set checkboxes
        self.useLoopback = QCheckBox("Use loopback")
        if ((self.flags & FLAGS_USE_LOOPBACK) != 0):
            self.useLoopback.setChecked(True)
        else:
            self.useLoopback.setChecked(False)

        self.joyHasLeds = QCheckBox("Joystick has LEDs")
        if ((self.flags & FLAGS_JOYSTICK_HAS_LEDS) != 0):
            self.joyHasLeds.setChecked(True)
        else:
            self.joyHasLeds.setChecked(False)

        self.joyLedsAreFirst = QCheckBox("Joystick LEDs are first")
        if ((self.flags & FLAGS_JOYSTICK_LEDS_ARE_FIRST) != 0):
            self.joyLedsAreFirst.setChecked(True)
        else:
            self.joyLedsAreFirst.setChecked(False)

        self.has4P = QCheckBox("Has 4P button")
        if ((self.flags & FLAGS_HAS_4P) != 0):
            self.has4P.setChecked(True)
        else:
            self.has4P.setChecked(False)

        self.fourPLightsAll = QCheckBox("4P lights all punches")
        if ((self.flags & FLAGS_FOUR_P_TURNS_ON_ALL_P) != 0):
            self.fourPLightsAll.setChecked(True)
        else:
            self.fourPLightsAll.setChecked(False)

        self.has4K = QCheckBox("Has 4K button")
        if ((self.flags & FLAGS_HAS_4K) != 0):
            self.has4K.setChecked(True)
        else:
            self.has4K.setChecked(False)

        self.fourKLightsAll = QCheckBox("4K lights all kicks")
        if ((self.flags & FLAGS_FOUR_K_TURNS_ON_ALL_K) != 0):
            self.fourKLightsAll.setChecked(True)
        else:
            self.fourKLightsAll.setChecked(False)

        self.hasExtraUp = QCheckBox("Has extra Up button")
        if ((self.flags & FLAGS_HAS_4P) != 0):
            self.hasExtraUp.setChecked(True)
        else:
            self.hasExtraUp.setChecked(False)

        self.upload = QPushButton("Update flags")
        self.upload.clicked.connect(lambda: self.setFlags())

        self.layout = QGridLayout()
        self.layout.addWidget(self.useLoopback, 0, 0)
        self.layout.addWidget(self.joyHasLeds, 0, 1)
        self.layout.addWidget(self.joyLedsAreFirst, 0, 2)
        self.layout.addWidget(self.has4P, 1, 0)
        self.layout.addWidget(self.fourPLightsAll, 1, 1)
        self.layout.addWidget(self.has4K, 2, 0)
        self.layout.addWidget(self.fourKLightsAll, 2, 1)
        self.layout.addWidget(self.hasExtraUp, 1, 2)
        self.layout.addWidget(self.upload, 2, 2)

        self.setLayout(self.layout)

    def setFlags(self):
        """Sends flag values to the board as one byte"""
        self.flags = 0
        if (self.useLoopback.isChecked()):
            self.flags += FLAGS_USE_LOOPBACK

        if (self.joyHasLeds.isChecked()):
            self.flags += FLAGS_JOYSTICK_HAS_LEDS

        if (self.joyLedsAreFirst.isChecked()):
            self.flags += FLAGS_JOYSTICK_LEDS_ARE_FIRST

        if (self.has4P.isChecked()):
            self.flags += FLAGS_HAS_4P

        if (self.fourPLightsAll.isChecked()):
            self.flags += FLAGS_FOUR_P_TURNS_ON_ALL_P

        if (self.has4K.isChecked()):
            self.flags += FLAGS_HAS_4K

        if (self.fourKLightsAll.isChecked()):
            self.flags += FLAGS_FOUR_K_TURNS_ON_ALL_K

        if (self.hasExtraUp.isChecked()):
            self.flags += FLAGS_HAS_EXTRA_UP_BUTTON
        
        commandSet(self.parent.portSelect.portsCombo.currentText(), "set flags {}".format(self.flags), self.flags)
        

class arcadeButton(QWidget):
    def __init__(self, parent, name, colorinfo):
        super(arcadeButton, self).__init__(parent)
        # Properties
        self.name = QLabel(name)
        self.parent = parent
        self.passiveColor = QColor(qRgb(colorinfo[0], colorinfo[1], colorinfo[2]))
        self.activeColor = QColor(qRgb(colorinfo[3], colorinfo[4], colorinfo[5]))

        # Content
        self.passiveColorLabel = QLabel("Not pressed: ")
        self.passiveColorButton = QPushButton("Click me !")
        self.passiveColorButton.clicked.connect(lambda: self.openColorDialog(0))
        self.passiveColorButton.setStyleSheet("color: rgb({}, {}, {}); background-color: rgb({}, {}, {})".format(self.passiveColor.red(), self.passiveColor.green(), self.passiveColor.blue(), \
                                                                                                                 self.passiveColor.red(), self.passiveColor.green(), self.passiveColor.blue()))
        self.activeColorLabel = QLabel("Pressed: ")
        self.activeColorButton = QPushButton("Click me !")
        self.activeColorButton.clicked.connect(lambda: self.openColorDialog(1))
        self.activeColorButton.setStyleSheet("color: rgb({}, {}, {}); background-color: rgb({}, {}, {})".format(self.activeColor.red(), self.activeColor.green(), self.activeColor.blue(), \
                                                                                                                 self.activeColor.red(), self.activeColor.green(), self.activeColor.blue()))

        # Layout
        self.mainBox = QVBoxLayout()
        self.subBox0 = QHBoxLayout()
        self.subBox1 = QHBoxLayout()
        self.subBox2 = QHBoxLayout()

        self.subBox0.addWidget(self.name)

        self.subBox1.addWidget(self.passiveColorLabel)
        self.subBox1.addWidget(self.passiveColorButton)

        self.subBox2.addWidget(self.activeColorLabel)
        self.subBox2.addWidget(self.activeColorButton)

        self.mainBox.addLayout(self.subBox0)
        self.mainBox.addLayout(self.subBox1)
        self.mainBox.addLayout(self.subBox2)

        # Display
        self.setLayout(self.mainBox)

    # Methods
    def openColorDialog(self, selector):
        """Opens a color picker and updates the relevant button data"""
        color = QColorDialog.getColor()
        if color.isValid():
            if selector == 0:
                self.passiveColor = color
                self.passiveColorButton.setStyleSheet("color: rgb({}, {}, {}); background-color: rgb({}, {}, {})".format(self.passiveColor.red(), self.passiveColor.green(), self.passiveColor.blue(), \
                                                                                                                 self.passiveColor.red(), self.passiveColor.green(), self.passiveColor.blue()))
            elif selector == 1:
                self.activeColor = color
                self.activeColorButton.setStyleSheet("color: rgb({}, {}, {}); background-color: rgb({}, {}, {})".format(self.activeColor.red(), self.activeColor.green(), self.activeColor.blue(), \
                                                                                                                 self.activeColor.red(), self.activeColor.green(), self.activeColor.blue()))

class MainWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("BadLED Control Deck")

        # Ports
        self.portSelect = portsWidget(self)

        # Values
        self.values = self.getHwInfo()

        # Buttons
        self.numberOfButtons = self.values[0];
        self.buttons = self.buildButtonArray()

        # Delay
        self.delay = delayWidget(self, self.values[1])

        # Flags
        self.flags = flagsWidget(self, self.values[2])

        # Panels
        self.panels = []
        self.panelLayouts = []
        self.buildPanels()

        # Needs to be connected here to update info when a new port is selected
        self.portSelect.portsCombo.currentIndexChanged.connect(lambda: self.changePort())
            
        # Layout
        self.mainVBox = QVBoxLayout()       # Main layout
        self.buttonsHBox = QHBoxLayout()    # Holds the panels that contain the arcadeButton widgets

        for i in range(len(self.panels)):
            self.buttonsHBox.addWidget(self.panels[i])

        self.mainVBox.addWidget(self.portSelect)
        self.mainVBox.addWidget(self.flags)
        self.mainVBox.addWidget(self.delay)
        self.mainVBox.addLayout(self.buttonsHBox)

        self.setLayout(self.mainVBox)

        # Show
        self.show()

    # Methods
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

        if not result:
            QMessageBox.critical(None, "Error", "No available serial devices have been found. Aborting.")
            sys.exit(0)
        else:
            return result

    def changePort(self):
        """Polls a new port and updates values accordingly"""
        c = self.getHwInfo()

        if c is not None:
            self.numberOfButtons = self.values[0];
            self.buttons = self.buildButtonArray()
            self.delay = delayWidget(self, self.values[1])
            self.flags = flagsWidget(self, self.values[2])            
        
    def getHwInfo(self):
        """Gets just about everything in a single command (number of buttons, delay, flags, and color info)."""
        return commandGet(self.portSelect.portsCombo.currentText(), "get hwinfo", 0)

    def buildPanels(self):
        """Builds button panels"""
        for i in range(int(self.numberOfButtons / 4)):
                self.panels.append(QFrame(self))
                self.panelLayouts.append(QVBoxLayout())

        for i in range(len(self.panelLayouts)):
            for j in range(i*4, i*4+4):
                self.panelLayouts[i].addWidget(self.buttons[j])
            self.panels[i].setLayout(self.panelLayouts[i])
            self.panels[i].setFrameStyle(QFrame.Raised | QFrame.Panel)

    def buildButtonArray(self):
        """Populates the array of arcadeButton widgets using color information provided by the hardware"""
        arr = []
        
        for i in range(self.numberOfButtons - 1):
            j = 3 + (i * 6)
            arr.append(arcadeButton(self, "LED {}".format(i+1), self.values[j:j+7]))
        return arr

def connect(portString):
    """Connects to a serial port. Since they are enumerated by a separate function, we assume the port exists and is available"""
    s = serial.Serial(portString, timeout=5)
    resp = s.readline()
    if resp.decode()[:-2] == "I'm a BadLED, duh":
        return s
    else:
        return None

def commandSet(portString, command, flags):
    """Sends a command through serial, does not expect a return."""
    s = connect(portString)
    if s == None:
        QMessageBox.critical(None, "Error", "Device on {} does not seem to be a BadLED controller".format(portString))
    else:
        s.write(command.encode())
        resp = s.readline()
        if ((flags & FLAGS_USE_LOOPBACK) != 0) == True:
            resp = s.readline()
        s.close()
        QMessageBox.information(None, "Done", "Upload successful !")
        return True

def commandGet(portString, command, flags):
    """Sends a command through serial, returns the hardware's answer."""
    s = connect(portString)
    if s == None:
        QMessageBox.critical(None, "Error", "Device on {} does not seem to be a BadLED controller".format(portString))
        return None
    else:
        s.write(command.encode())
        resp = s.readline()
        if ((flags & FLAGS_USE_LOOPBACK) != 0) == True:
            resp = s.readline()
        s.close()
        return resp

if __name__ == '__main__':
    app = QApplication(sys.argv)
    context = MainWindow()
    sys.exit(app.exec_())
