CXX = g++
CFLAGS = -Wall -fPIC -O2
LDFLAGS =
INCLUDES = /usr/include/qt4


TARGET_1 = ds_passkeeper

SRC_1 = MainWindow.cpp \
        Randomizer.cpp \
        StorageEngine.cpp \
        moc_MainWindow.cpp \
        passkeeper-main.cpp

LIBS_1 = QtCore \
         QtGui \
         crypto


TARGET_2 = ds_randomgen

SRC_2 = Randomizer.cpp \
        randomgen-main.cpp

LIBS_2 = crypto

INCDIR = include
SRCDIR = src
OBJDIR = obj
OUTDIR = bin

VPATH = $(SRCDIR)
INCPATHS = $(INCDIR) $(INCLUDES)

OBJECTS_1 = $(patsubst %.cpp, $(OBJDIR)/%.o, $(SRC_1))
OBJECTS_2 = $(patsubst %.cpp, $(OBJDIR)/%.o, $(SRC_2))


all: directories $(TARGET_1) $(TARGET_2)

$(TARGET_1): $(OBJECTS_1)
	$(CXX) $(OBJECTS_1) $(LDFLAGS) $(addprefix -l, $(LIBS_1)) -o $(OUTDIR)/$(TARGET_1)

$(TARGET_2): $(OBJECTS_2)
	$(CXX) $(OBJECTS_2) $(LDFLAGS) $(addprefix -l, $(LIBS_2)) -o $(OUTDIR)/$(TARGET_2)

$(OBJDIR)/%.o: %.cpp
	$(CXX) -c $(CFLAGS) $(LDFLAGS) $(addprefix -I, $(INCPATHS)) $< -o $@

moc_%.cpp : $(INCDIR)/%.h
	moc-qt4 $< -o $@

directories:
	mkdir -p $(OBJDIR)
	mkdir -p $(OUTDIR)

clean:
	rm -rf $(OBJDIR) $(OUTDIR)

.PHONY: clean directories $(TARGET_1) $(TARGET_2) all
