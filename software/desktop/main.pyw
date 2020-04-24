# Qt
from PyQt5.QtWidgets import QApplication, QMessageBox, QWidget
from PyQt5.QtWidgets import QVBoxLayout, QHBoxLayout, QGridLayout, QFrame
from PyQt5.QtWidgets import QColorDialog, QComboBox, QLabel, QPushButton, QSpinBox
from PyQt5.QtWidgets import QCheckBox
from PyQt5.QtGui import QIcon, QColor, QPalette, qRgb
from PyQt5 import QtCore

# Serial
import serial, serial.tools.list_ports

# Other
import glob, sys

# Flags pseudo defines
FLAGS_RESET_EEPROM            = 128
FLAGS_USE_LOOPBACK            = 64
FLAGS_LEDS_STATE              = 32
FLAGS_STATE_UNUSED_BIT_1      = 16
FLAGS_STATE_UNUSED_BIT_2      = 8
FLAGS_STATE_UNUSED_BIT_3      = 4
FLAGS_STATE_UNUSED_BIT_4      = 2
FLAGS_STATE_UNUSED_BIT_5      = 1

FLAGS_JOYSTICK_HAS_LEDS       = 128
FLAGS_HAS_4P                  = 64
FLAGS_FOUR_P_TURNS_ON_ALL_P   = 32
FLAGS_FOUR_P_SHARES_COLOR     = 16
FLAGS_HAS_4K                  = 8
FLAGS_FOUR_K_TURNS_ON_ALL_K   = 4
FLAGS_FOUR_K_SHARES_COLOR     = 2
FLAGS_HAS_EXTRA_UP_BUTTON     = 1

class ExceptionHandler(QtCore.QObject):
    errorSignal = QtCore.pyqtSignal()

    def __init__(self):
        super(ExceptionHandler, self).__init__()

    def handler(self, exctype, value, traceback):
        self.errorSignal.emit()
        sys._excepthook(exctype, value, traceback)

exceptionHandler = ExceptionHandler()
sys._excepthook = sys.excepthook
sys.excepthook = exceptionHandler.handler

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
        commandSet(self.parent.portSelect.portsCombo.currentText(), "set delay {}".format(self.spinbox.value()), self.parent.flags.stateFlags, self.parent.flags.layoutFlags)

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

        values = values[:-1] # Remove trailing space            
        commandSet(self.portsCombo.currentText(), values, self.parent.flags.stateFlags, self.parent.flags.layoutFlags)

    def buildPortsComboBox(self, arr):
        """Creates a combo box containing the list of available serial ports."""
        for i in range(len(arr)):
            self.portsCombo.addItem(arr[i])      

class flagsWidget(QWidget):
    def __init__(self, parent, stateFlags, layoutFlags):
        super(flagsWidget, self).__init__(parent)
        self.parent = parent
        self.stateFlags = stateFlags
        self.layoutFlags = layoutFlags

        # Set checkboxes
        self.joyHasLeds = QCheckBox("Joystick has LEDs")
        if ((self.layoutFlags & FLAGS_JOYSTICK_HAS_LEDS) != 0):
            self.joyHasLeds.setChecked(True)
        else:
            self.joyHasLeds.setChecked(False)

        self.has4P = QCheckBox("Has 4P button")
        if ((self.layoutFlags & FLAGS_HAS_4P) != 0):
            self.has4P.setChecked(True)
        else:
            self.has4P.setChecked(False)

        self.fourPLightsAll = QCheckBox("4P lights all punches")
        if ((self.layoutFlags & FLAGS_FOUR_P_TURNS_ON_ALL_P) != 0):
            self.fourPLightsAll.setChecked(True)
        else:
            self.fourPLightsAll.setChecked(False)

        self.fourPSharesColor = QCheckBox("4P shares color")
        if ((self.layoutFlags & FLAGS_FOUR_P_SHARES_COLOR) != 0):
            self.fourPSharesColor.setChecked(True)
        else:
            self.fourPSharesColor.setChecked(False)

        self.has4K = QCheckBox("Has 4K button")
        if ((self.layoutFlags & FLAGS_HAS_4K) != 0):
            self.has4K.setChecked(True)
        else:
            self.has4K.setChecked(False)

        self.fourKLightsAll = QCheckBox("4K lights all kicks")
        if ((self.layoutFlags & FLAGS_FOUR_K_TURNS_ON_ALL_K) != 0):
            self.fourKLightsAll.setChecked(True)
        else:
            self.fourKLightsAll.setChecked(False)

        self.fourKSharesColor = QCheckBox("4K shares color")
        if ((self.layoutFlags & FLAGS_FOUR_K_SHARES_COLOR) != 0):
            self.fourKSharesColor.setChecked(True)
        else:
            self.fourKSharesColor.setChecked(False)

        self.hasExtraUp = QCheckBox("Has extra Up button")
        if ((self.layoutFlags & FLAGS_HAS_4P) != 0):
            self.hasExtraUp.setChecked(True)
        else:
            self.hasExtraUp.setChecked(False)

        self.upload = QPushButton("Update flags")
        self.upload.clicked.connect(lambda: self.setFlags())

        self.layout = QGridLayout()
        self.layout.addWidget(self.has4P, 0, 0)
        self.layout.addWidget(self.fourPLightsAll, 0, 1)
        self.layout.addWidget(self.fourPSharesColor, 0, 2)
        self.layout.addWidget(self.has4K, 1, 0)
        self.layout.addWidget(self.fourKLightsAll, 1, 1)
        self.layout.addWidget(self.fourKSharesColor, 1, 2)
        self.layout.addWidget(self.joyHasLeds, 2, 0)
        self.layout.addWidget(self.hasExtraUp, 2, 1)
        self.layout.addWidget(self.upload, 2, 2)

        self.setLayout(self.layout)

    def setFlags(self):
        """Sends flag values to the board as two bytes"""
        self.layoutFlags = 0
        if (self.joyHasLeds.isChecked()):
            self.layoutFlags += FLAGS_JOYSTICK_HAS_LEDS

        if (self.has4P.isChecked()):
            self.layoutFlags += FLAGS_HAS_4P
            
        if (self.fourPLightsAll.isChecked()):
            self.layoutFlags += FLAGS_FOUR_P_TURNS_ON_ALL_P

        if (self.fourPSharesColor.isChecked()):
            self.layoutFlags += FLAGS_FOUR_P_SHARES_COLOR

        if (self.has4K.isChecked()):
            self.layoutFlags += FLAGS_HAS_4K

        if (self.fourKLightsAll.isChecked()):
            self.layoutFlags += FLAGS_FOUR_K_TURNS_ON_ALL_K

        if (self.fourKSharesColor.isChecked()):
            self.layoutFlags += FLAGS_FOUR_K_SHARES_COLOR

        if (self.hasExtraUp.isChecked()):
            self.layoutFlags += FLAGS_HAS_EXTRA_UP_BUTTON

        commandSet(self.parent.portSelect.portsCombo.currentText(), "set flags {} {}".format(self.stateFlags, self.layoutFlags), self.stateFlags, self.layoutFlags)

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
        self.values = [ int(x) for x in self.getHwInfo().decode().split(" ")[:-1] ]

        # Buttons
        self.numberOfButtons = self.values[0]
        self.buttons = self.buildButtonArray()

        # Delay
        self.delay = delayWidget(self, self.values[1])

        # Flags
        self.flags = flagsWidget(self, self.values[2], self.values[3])

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
            # Delete old layouts
            for i in reversed(range(len(self.panelLayouts))):
                for j in reversed(range(0, 4)):
                    self.panelLayouts[i].itemAt(j).widget().setParent(None)

            for i in reversed(range(len(self.panels))):
                del self.panels[i]

            for i in reversed(range(len(self.buttons))):
                del self.buttons[i]

            del self.panelLayouts
            del self.panels
            del self.buttons
                
            # Get new values
            self.values = [ int(x) for x in self.getHwInfo().decode().split(" ")[:-1] ]

            self.numberOfButtons = self.values[0];
            self.buttons = self.buildButtonArray()

            self.delay = delayWidget(self, self.values[1])

            self.flags = flagsWidget(self, self.values[2])
                
            self.panels = []
            self.panelLayouts = []
            self.buildPanels()

            # Re-add everything
            self.mainVBox = QVBoxLayout()
            self.buttonsHBox = QHBoxLayout()

            for i in range(len(self.panels)):
                self.buttonsHBox.addWidget(self.panels[i])

            self.mainVBox.addWidget(self.flags)
            self.mainVBox.addWidget(self.delay)
            self.mainVBox.addLayout(self.buttonsHBox)

            self.setLayout(self.mainVBox)

            # Show
            self.show()
        
    def getHwInfo(self):
        """Gets just about everything in a single command (number of buttons, delay, flags, and color info)."""
        return commandGet(self.portSelect.portsCombo.currentText(), "get hwinfo", 0, 0)

    def buildPanels(self):
        """Builds button panels"""
        for i in range(int(self.numberOfButtons / 4)):
                self.panels.append(QFrame(self))
                self.panelLayouts.append(QVBoxLayout())

        for i in range(len(self.panelLayouts)):
            for j in range(i*4, (i*4+4)):
                try:
                    self.panelLayouts[i].addWidget(self.buttons[j])
                except Exception as e:
                    pass
            self.panels[i].setLayout(self.panelLayouts[i])
            self.panels[i].setFrameStyle(QFrame.Raised | QFrame.Panel)

    def buildButtonArray(self):
        """Populates the array of arcadeButton widgets using color information provided by the hardware"""
        arr = []

        for i in range(self.numberOfButtons):
            j = 4 + (6 * i)
            arr.append(arcadeButton(self, "LED {}".format(i+1), self.values[j:j+6]))
        return arr

def connect(portString):
    """Connects to a serial port. Since they are enumerated by a separate function, we assume the port exists and is available"""
    s = serial.Serial(portString, timeout=5)
    resp = s.readline()
    if resp.decode()[:-2] == "I'm a BadLED, duh":
        return s
    else:
        print(resp)
        return None

def commandSet(portString, command, stateFlags, layoutFlags):
    """Sends a command through serial, does not expect a return."""
    s = connect(portString)
    if s == None:
        QMessageBox.critical(None, "Error", "Device on {} does not seem to be a BadLED controller".format(portString))
    else:
        s.write(command.encode())
        resp = s.readline()
        if ((stateFlags & FLAGS_USE_LOOPBACK) != 0) == True:
            print(resp.decode(), end="")
            resp = s.readline()
        s.close()
        QMessageBox.information(None, "Done", "Upload successful !")
        return True

def commandGet(portString, command, stateFlags, layoutFlags):
    """Sends a command through serial, returns the hardware's answer."""
    s = connect(portString)
    if s == None:
        QMessageBox.critical(None, "Error", "Device on {} does not seem to be a BadLED controller".format(portString))
        return None
    else:
        s.write(command.encode())
        resp = s.readline()
        if ((stateFlags & FLAGS_USE_LOOPBACK) != 0) == True:
            print(resp.decode(), end="")
            resp = s.readline()
        s.close()
        return resp

if __name__ == '__main__':
    app = QApplication(sys.argv)
    context = MainWindow()
    sys.exit(app.exec_())
