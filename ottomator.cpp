/*
* ottomator.cpp
* Ottomator100
*
* Created by Louis CHEN on 03/08/15
* Copyright (c) 2015 ITECH INSTRUMENTS. All rights reserved.
*/

#include <iostream>
#include <winsock2.h>
#include <windows.h>
#include "ottomator.h"

using namespace std;

Ottomator::Ottomator()
{
    // Init status matrix
    resetM_StatusMatrix();

    writeLog("The Ottomator is created.\n");
}


Ottomator::~Ottomator()
{
    writeLog("The Ottomator is dead.\n");
}


/// Utilities
/// ---------

string Ottomator::getAllStatusesOfDSS1ForBit(int index, bool verbose)
{
    string statuses("");
    for(int actuator=1; actuator < 5; ++actuator)
    {
        m_robot.updateStatusOfCurrent(actuator);
        statuses.append(OttoUtils::numberToString(m_robot.getDeviceDataStatusRegister1Bit(index)));
    }
    if(verbose)
    {
        //cout << "The " << symbols[index] << " statuses are:" << endl;
        writeLog("The " + symbols[index] + " statuses are:\n");
        //cout << "x \t" << "y \t" << "z \t" << "p" << endl; // "v" is special, not included in this method
        writeLog("x \ty \tz \tp \n");
        for(unsigned int i=0; i < statuses.length(); ++i)
        {
            //cout << statuses[i] << "\t";
            writeLog(OttoUtils::numberToString(statuses[i])+"\t");
        }
    }

    //if(verbose) cout << endl;
    if(verbose) writeLog("\n");
    return statuses;

}

void Ottomator::updateM_StatusMatrix() // Updates from live robot only. Special statuses are let untouched.
{
  int actuator;
    for( actuator=1; actuator < 6; ++actuator) // 1..5
    {
        m_robot.updateStatusOfCurrent(actuator);
        for(int i=1; i < 16; ++i) // 1..15
        {
            m_statusMatrix[i][actuator] = m_robot.getDeviceDataStatusRegister1Bit(i);
        }
        Sleep(100);
    }
    for( actuator=1; actuator < 5; ++actuator) // 1..4
    {
        m_statusMatrix[0][actuator] = m_robot.getRobotPosition(actuator);
    }
}

void Ottomator::resetM_StatusMatrix()
{   // reset statuses
    //nCurrentSample = 0; ///m_statusMatrix[0][0]
    //isFetching = 1; ///m_statusMatrix[1][0]
    //isInitiating = 1; ///m_statusMatrix[2][0]
    //birthSequenceCase = 1; ///m_statusMatrix[3][0]
    //frameworkSequenceCase = 1; ///m_statusMatrix[4][0]
    //workSequenceCase = 1; ///m_statusMatrix[5][0]

    for(int actuator=0; actuator < 6; ++actuator) // 0..5 (Actuator 0 is reserved for special statuses)
    {
        for(int i=0; i < 16; ++i) // 0..15
        {
            m_statusMatrix[i][actuator] = 0;
        }
    }
    for(int i=1; i < 6; ++i) m_statusMatrix[i][0] = 1; // 1..5
}

int Ottomator::manageStatusGate()
{
  int i;
    int issueMessage(0);
    int index(0), normalValue(0), weight(0);
    bool rec;
    // Get status matrix
    updateM_StatusMatrix();
    displayM_StatusMatrix(); /// test

    // Gate checks IAI
    for(i=0; i<6; ++i)
    {
        rec = false;
        switch(i)
        {
            case 0: // EMGS?
                index = 15; normalValue = 0; weight = EMGS;
                break;
            case 1: // ALMH?
                index = 10; normalValue = 0; weight = ALMH;
                break;
            case 2: // ALML?
                index = 9; normalValue = 0; weight = ALML;
                break;
            case 3: // ABER?
                index = 8; normalValue = 0; weight = ABER;
                break;
            case 4: // STP?
                index = 5; normalValue = 0; weight = STP;
                break;
            case 5: // SV?
                index = 12; normalValue = 1; weight = SV;
        }
        for(int actuator=1; actuator < 5 ; ++actuator)
        {
            if(m_statusMatrix[index][actuator] != normalValue) rec = true;
        }
        // record issue
        if (rec) issueMessage += weight;
    }

    // Gate checks V
    for(i=0; i<5; ++i)
    {
        switch(i)
        {
            case 0: // EMGV?
                index = 2; normalValue = 1; weight = EMGV;
                break;
            case 1: // MOTO?
                index = 5; normalValue = 0; weight = MOTO;
                break;
            case 2: // P1?
                index = 3; normalValue = 0; weight = P1;
                break;
            case 3: // P2?
                index = 4; normalValue = 0; weight = P2;
                break;
            case 4: // LAT?
                index = 1; normalValue = 1; weight = LAT;
                break;
        }

        if(m_statusMatrix[index][5] != normalValue)
        {   // record issue
            issueMessage += weight;
        }

    }

    // Health checks
    // ...

    return issueMessage;
}


int Ottomator::manageNextFor(int sequence, int actionSuccess, int error, int specialIndex, int specialManagement)
{
    if(specialIndex != 0) sequence+=3;
    if(specialManagement > 0)
    {   // Special management
        if(specialManagement == 1 && actionSuccess == Position_completed) // if x goes to castel and PEND
        {   // then fatal error
            return XCAS;
        }

        // if p goes to close and PSFL then empty catch
        //... (not implemented for the moment)

    }
    if(actionSuccess > 0) // 1 = position completed / catch successful (PEND), 2 = push completed without obstacle (PSFL Missed work part)
    {
        switch(sequence)
        {
            // normal cases
            case 1:
                ++m_statusMatrix[3][0];
                break;
            case 2:
                ++m_statusMatrix[4][0];
                break;
            case 3:
                ++m_statusMatrix[5][0];
                break;
            // spacial cases
            case 4:
                m_statusMatrix[3][0] = specialIndex;
                break;
            case 5:
                m_statusMatrix[4][0] = specialIndex;
                break;
            case 6:
                m_statusMatrix[5][0] = specialIndex;
        }
        error = 0;
    } else
    {
        ++error;
    }
    return error;
}

bool Ottomator::allServo(bool onOff)
{
    bool ret(1);
    for(int actuator=1; actuator<5; ++actuator) // 1..4
    {
        ret &= m_robot.servo(onOff, actuator);
    }
    return ret;
}

/// Main Sequences
/// --------------

/// func --- 1
int Ottomator::birthSequence()
{
    int error(0), sequence(1), i;
    int intMessage;

    do
    {
        intMessage = manageStatusGate();
        if((intMessage & (~1)) != 0) return intMessage; // 000 0000 0001 (Only SV abnormal) OR 000 0000 0000 (All normal) // partsToDisable = 1;

        switch(m_statusMatrix[3][0])
        {
            // Activate MODBUS
            /*
            *case 0:
            *    if(m_robot.activateMODBUS(1) && m_robot.activateMODBUS(2) && m_robot.activateMODBUS(3) && m_robot.activateMODBUS(4) && m_robot.activateMODBUS(5))
            *    {
            *        ++m_statusMatrix[3][0];
            *        //cout << "MODBUS Activated" << endl;
            *    } else {
            *        //cout << "Ottomator > [Error] : MODBUS Activation failed" << endl;
            *    }
            *    break;
            */
            // Servo ON
            case 1:
                if(!(m_statusMatrix[12][1] && m_statusMatrix[12][2] && m_statusMatrix[12][3] && m_statusMatrix[12][4]))
                {
                    if(allServo(1))
                    {
                        ++m_statusMatrix[3][0]; error = 0;
                        writeLog("All Servo ON\n");
                    } else
                    {
                        ++ error;
                        writeLog("Ottomator > [Error] : Servo ON failed\n");
                        getAllStatusesOfDSS1ForBit(12, true); // SV: Servo ON status
                    }
                } else
                {
                    ++m_statusMatrix[3][0];
                    writeLog("All Servo already ON\n");
                }
                break;

            // Si besoin homing, Homing
            case 2:
                int h;
                for(i=1; i < 5; ++i) // 1..4
                {
                    h += m_statusMatrix[4][i];
                }
                if(h == 4)
                {
                    m_statusMatrix[3][0] = 11; // birth completion flag
                    writeLog("Birth completed\n");
                    return Birth_completed;
                } else
                {
                    ++ m_statusMatrix[3][0];
                }
                break;
            case 3:
                if(m_statusMatrix[4][3]) // HEND Home return complete, thus homing is useless
                {
                    error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::zAct, 1), error); // Remontée Z
                } else
                {
                    error = manageNextFor(sequence, m_robot.homing(OttoUtils::zAct), error); // Homing Z
                }
                break;
            case 4:
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::yAct, yOfSample(POSITION_REST)), error); // Safe position Y for Z actuator
                break;
            case 5:
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::vAct, 1), error); // Ouverture porte château
                break;
            case 6:
                if(m_statusMatrix[4][1]) // HEND Home return complete, thus homing is useless
                {
                    ++m_statusMatrix[3][0]; error = 0;
                } else
                {
                    error = manageNextFor(sequence, m_robot.homing(OttoUtils::xAct), error); // Homing X
                }
                break;
            case 7:
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::xAct, xOfSample(0)), error); // Positionnement emplacement Default en x
                break;
            case 8:
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::yAct, yOfSample(0)), error); // Positionnement emplacement Default en y
                break;
            case 9:
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::zAct, 2), error); // Descente Z Table // Now it is safe for homing P
                break;
            case 10:
                m_robot.updateStatusOfCurrent(OttoUtils::pAct);
                if(m_robot.getDeviceDataStatusRegister1Bit(4)) // HEND Home return complete, thus homing is useless
                {
                    error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::pAct, 1), error); // Ouverture Pince
                } else
                {
                    error = manageNextFor(sequence, m_robot.homing(OttoUtils::pAct), error); // Homing P
                }
                if(error == 0)
                {
                    m_statusMatrix[3][0] = 11; // birth completion flag
                    writeLog("Birth completed\n");
                    return Birth_completed;
                }
                break;
            default:
                writeLog("Ottomator > [Error] m_statusMatrix[3][0] is out of range = " + OttoUtils::numberToString(m_statusMatrix[3][0]) + "\n");
                return Sequence_case_out_of_range;
        }
    } while(error < 5);
    writeLog("Ottomator > [Error] Birth sequence stuck on case " + OttoUtils::numberToString(m_statusMatrix[3][0]) + "\n");
    return Stuck_in_birth_sequence;
}


/// func --- 2
int Ottomator::frameworkSequenceTo(bool initiate) // Initiate / Finish
{
    m_statusMatrix[2][0] = initiate;
    int error(0), sequence(2);
    int intMessage;
    stringstream ss;
    string temp, readData;
    int found;
    bool closePlier;

    do
    {
        intMessage = manageStatusGate();
        if(intMessage != 0) return intMessage;

        switch(m_statusMatrix[4][0])
        {
            // Check if plier is emplty
            case 1:
                // If somethingInPlier, fermeture pince & next. Else, ouverture pince & Goto to Default position
                readData = m_robot.m_busManager.modbusReadData(OttoUtils::pAct, 0x03, 0x9000, 0x0002, 10);
                ss << readData;
                while(getline(ss, temp, '.'))
                {
                    if(stringstream(temp) >> found)
                    {   // write buffer
                        closePlier = (found > 300);
                    }
                }
                // Fermeture Pince OR Ouverture Pince -> puis aller à la position par défaut
                (closePlier) ? error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::pAct, 2), error) : error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::pAct, 1), error, 7);
                break;
            // Putback current sample (!nothingInPlier)
            case 2:
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::zAct, 1), error); // Remontée Z
                break;
            case 3:
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::xAct, xOfSample(m_statusMatrix[0][0])), error); // Positionnement emplacement m_statusMatrix[0][0] en x
                break;
            case 4:
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::yAct, yOfSample(m_statusMatrix[0][0])), error); // Positionnement emplacement m_statusMatrix[0][0] en y
                break;
            case 5:
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::zAct, 2), error); // Descente Z Table
                break;
            case 6:
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::pAct, 1), error); // Ouverture Pince
                break;

            // Default position
            case 7:
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::zAct, 1), error); // Remontée Z
                break;
            case 8:
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::xAct, xOfSample(0)), error); // Positionnement emplacement Default en x
                break;
            case 9:
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::yAct, yOfSample(0)), error); // Positionnement emplacement Default en y
                break;

            // End Init or Close castel
            case 10:
                if(m_statusMatrix[2][0])
                {
                    m_statusMatrix[4][0] = 1; m_statusMatrix[2][0] = -1; // Initiation completion flag
                    writeLog("Initiation completed\n");
                    return Initiation_completed;
                }
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::vAct, 2), error); // Fermeture porte château
                break;

            // Servo OFF
            case 11:
            if(allServo(0))
            {
                error = 0;
                resetM_StatusMatrix(); // Batch completed, reset statuses
                m_sampleArray.clear(); // Clear task list
                writeLog("Finish completed\n"); // Château fermé, servoOFF : Fin de la séquence.
                return Finish_completed;
            } else
            {
                //cout << "Ottomator > [Error] : Servo OFF failed" << endl;
                writeLog("Ottomator > [Error] : Servo OFF failed\n");
                getAllStatusesOfDSS1ForBit(12, true); // SV: Servo ON status
            }
        }
    } while(error < 5);
    writeLog("Ottomator > [Error] Framework sequence stuck on case " + OttoUtils::numberToString(m_statusMatrix[4][0]) + ", isInitiating = " + OttoUtils::numberToString(m_statusMatrix[2][0]) + "\n");
    if(m_statusMatrix[2][0]) // isInitiating
    {
        return Stuck_in_initiation_sequence;
    } else
    {
        return Stuck_in_finish_sequence;
    }
}


/// func --- 3
int Ottomator::workSequenceTo(bool fetch, int sampleN) // bool fetch = true; Cas IW demande à chercher l'échantillon (false = reposer) // int sampleN = 1; Cas échantillon 1
{
    // Initiate work
    m_statusMatrix[1][0] = fetch;
    m_statusMatrix[0][0] = sampleN;
    int error(0), sequence(3);
    int intMessage;

    /// workSequence
    /// while ---
    do
    {
        intMessage = manageStatusGate();
        if(intMessage != 0) return intMessage;

        switch(m_statusMatrix[5][0])
        {
            // Prepare
            case 1:
                // Check if x position is safe

                // perform
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::vAct, 1), error); // Ouverture porte château
                break;
            case 2:
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::zAct, 1), error); // Remontée Z
                break;

            // Motion 1
            case 3:
                if(m_statusMatrix[1][0])
                {	// fetch
                    error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::xAct, xOfSample(m_statusMatrix[0][0])), error); // Positionnement emplacement m_statusMatrix[0][0] en x
                } else
                {	// putback
                    error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::yAct, yOfSample(POSITION_DETECTOR)), error); // Positionnement emplacement Château en y
                }
                break;
            case 4:
                if(m_statusMatrix[1][0])
                {	// fetch
                    error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::yAct, yOfSample(m_statusMatrix[0][0])), error); // Positionnement emplacement m_statusMatrix[0][0] en y
                } else
                {	// putback
                    error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::xAct, xOfSample(POSITION_DETECTOR)), error, 0, 1); // Positionnement emplacement Château en x
                    if(error == XCAS) {m_robot.setPosition(OttoUtils::xAct, xOfSample(POSITION_ZERO)); return XCAS;} // fatal error
                }
                break;

            // Pick
            case 5:
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::pAct, 1), error); // Ouverture Pince
                break;
            case 6:
                int destination;
                destination = (m_statusMatrix[1][0]) ? 2 : 3; // fetch => Descente Z Table // putback => Descente Z Château
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::zAct, destination), error); // Descente Z
                break;
            case 7:
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::pAct, 2), error); // Fermeture Pince // PEND => 1; // PSFL => 2; ### emptyCatch ??? ###
                break;
            case 8:
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::zAct, 1), error); // Remontée Z
                //if(positionCompletedFor("z") && !emptyCatch) ++m_statusMatrix[5][0]; // Z remonté avec objet
                //if(emptyCatch) return "Error: Empty catch"; // Z remonté sans objet
                break;

            // Motion 2
            case 9:
                if(m_statusMatrix[1][0])
                {	// fetch
                    error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::yAct, yOfSample(POSITION_DETECTOR)), error); // Positionnement emplacement Château en y
                } else
                {	// putback
                    error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::xAct, xOfSample(m_statusMatrix[0][0])), error); // Positionnement emplacement m_statusMatrix[0][0] en x
                }
                break;
            case 10:
                if(m_statusMatrix[1][0])
                {	// fetch
                    error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::xAct, xOfSample(POSITION_DETECTOR)), error, 0, 1); // Positionnement emplacement Château en x
                    if(error == XCAS) {m_robot.setPosition(OttoUtils::xAct, xOfSample(POSITION_ZERO)); return XCAS;} // fatal error
                } else
                {	// putback
                    error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::yAct, yOfSample(m_statusMatrix[0][0])), error); // Positionnement emplacement m_statusMatrix[0][0] en y
                }
                break;

            // Put
            case 11:
                destination = (m_statusMatrix[1][0]) ? 3 : 2; // fetch => Descente Z Château // putback => Descente Z Table
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::zAct, destination), error); // Descente Z
                break;
            case 12:
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::pAct, 1), error); // Ouverture Pince
                break;
            case 13:
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::zAct, 1), error); // Remontée Z
                break;

            // Position d'attente
            case 14:
                if(!m_statusMatrix[1][0])
                {
                    m_statusMatrix[5][0] = 1; m_statusMatrix[1][0] = 1; // putback completion flag
                    if(m_sampleArray.size() > 1)
                    {
                        m_sampleArray.erase(m_sampleArray.begin()); // updated m_sampleArray
                    } else
                    {
                        m_statusMatrix[2][0] = 0; // isInitiating = 0
                    }
                    writeLog("Putback completed\n"); // Echantillon reposé : Fin de la séquence.
                    return Putback_completed;
                }
                // fetch
                error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::xAct, xOfSample(POSITION_REST)), error); // Positionnement emplacement Attente en x
                break;

            // Finish
            case 15:
                if(m_statusMatrix[1][0])
                {	// fetch
                    error = manageNextFor(sequence, m_robot.setPosition(OttoUtils::vAct, 2), error); // Fermeture porte château
                    if(error == 0)
                    {
                        m_statusMatrix[5][0] = 1; m_statusMatrix[1][0] = 0; // fetch completion flag
                        writeLog("Fetch completed\n"); // Château fermé : Fin de la séquence.
                        return Fetch_completed;
                    }
                }
                break;
        }
    } while(error < 5);
    writeLog("Ottomator > [Error] Work sequence stuck on case " + OttoUtils::numberToString(m_statusMatrix[5][0]) + ", isFetching = " + OttoUtils::numberToString(m_statusMatrix[1][0]) +"\n");
    if(m_statusMatrix[1][0]) // isFetching
    {
        return Stuck_in_fetch_sequence;
    } else
    {
        return Stuck_in_putback_sequence;
    }
}



/// Demontrators
/// ------------

int Ottomator::cycler()
{
    int message;
    while(m_sampleArray.size() > 0)
    {
        if(m_statusMatrix[3][0] != 11)
        {
            message = birthSequence(); // birthSequence()
            if(message != Birth_completed) return message;
        } else
        {
            switch(m_statusMatrix[2][0])
            {
                case 1:
                    message = frameworkSequenceTo(true); // frameworkSequenceTo(initiate)
                    if(message != Initiation_completed) return message;
                    break;
                case -1:
                    // cout << "############################################## " << atoi(m_sampleArray[0].c_str()) << endl; /// test
                    if(m_statusMatrix[1][0] == 1)
                    {
                        return workSequenceTo(true, atoi(m_sampleArray[0].c_str())); // workSequenceTo(fetch, new nCurrentSample)
                    } else
                    {
                        message = workSequenceTo(false, m_statusMatrix[0][0]); // workSequenceTo(putback, old nCurrentSample)
                        if(message != Putback_completed) return message;
                    }
                    break;
                case 0:
                    return frameworkSequenceTo(false); // frameworkSequenceTo(finish)
            }
        }
    }
    writeLog("Need input\n");
    return Need_input;
}


void Ottomator::displayM_StatusMatrix()
{
    string s1, s2;
    for(int i=0; i<16; ++i)
    {
        (i<10) ? s1="  " : s1=" ";
        //cout << i << s1;
        writeLog(OttoUtils::numberToString(i)+s1);
        for(int j=0; j<6; ++j)
        {
            (m_statusMatrix[i][j]<10 && m_statusMatrix[i][j]>-1) ? s2="__" : s2="_";
            //cout << s2 << m_statusMatrix[i][j] ;
            writeLog(s2+OttoUtils::numberToString(m_statusMatrix[i][j]));
        }
        //cout << endl;
        writeLog("\n");
    }
}

/// Case getters and setters
/// ------------------------
// to access attributs...

int Ottomator::getBirthSequenceCase()
{
    return m_statusMatrix[3][0];
}
void Ottomator::setBirthSequenceCase(int n)
{
    m_statusMatrix[3][0] = n;
}

int Ottomator::getFrameworkSequenceCase()
{
    return m_statusMatrix[4][0];
}
void Ottomator::setFrameworkSequenceCase(int n)
{
    m_statusMatrix[4][0] = n;
}

int Ottomator::getWorkSequenceCase()
{
    return m_statusMatrix[5][0];
}
void Ottomator::setWorkSequenceCase(int n)
{
    m_statusMatrix[5][0] = n;
}

vector<string> Ottomator::getM_sampleArray()
{
    return m_sampleArray;
}
void Ottomator::setM_sampleArray(vector<string> sampleArray)
{
    m_sampleArray = sampleArray;
}

int Ottomator::getNCurrentSample()
{
    return m_statusMatrix[0][0];
}
void Ottomator::setNCurrentSample(int n)
{
    m_statusMatrix[0][0] = n;
}

void Ottomator::xyOfSample(int n, int& x, int& y)
{
  x=n%5;
  if(x == 0) x = 5;
  if(n == POSITION_REST    ) x = 4;
  if(n == POSITION_DETECTOR) x = 6;
  if(n == POSITION_ZERO    ) x = 1;

  y=(n+4+5)/5;
  if (n==POSITION_REST || n==POSITION_DETECTOR) y=6;
}

const char* Ottomator::GetErrorTextFrench(int i)
{
  switch (i) {
    default: return NULL;
  }
}
