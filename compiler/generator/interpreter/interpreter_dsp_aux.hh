/************************************************************************
 ************************************************************************
    FAUST compiler
    Copyright (C) 2003-2015 GRAME, Centre National de Creation Musicale
    ---------------------------------------------------------------------
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 ************************************************************************
 ************************************************************************/
 
#ifndef interpreter_dsp_aux_h
#define interpreter_dsp_aux_h

#include "faust/dsp/dsp.h"
#include "faust/gui/UI.h"
#include "faust/gui/meta.h"
#include "export.hh"
#include "fir_interpreter.hh"
#include "smartpointer.h"

#define INTERP_VERSION 0.5

class interpreter_dsp;

struct EXPORT interpreter_dsp_factory {
    
    std::string fExpandedDSP;
    std::string fShaKey;
    std::string fName;
    
    float fVersion;
    int fNumInputs;
    int fNumOutputs;
    
    int fIntHeapSize;
    int fRealHeapSize;
    int fSROffset;
    int fCountOffset;
    
    FIRUserInterfaceBlockInstruction<float>* fUserInterfaceBlock;
    FIRBlockInstruction<float>* fInitBlock;
    FIRBlockInstruction<float>* fComputeBlock;
    FIRBlockInstruction<float>* fComputeDSPBlock;
    
    interpreter_dsp_factory(const std::string& name,
                            float version_num,
                            int inputs, int ouputs,
                            int int_heap_size, int real_heap_size,
                            int sr_offset, int count_offset,
                            FIRUserInterfaceBlockInstruction<float>* interface,
                            FIRBlockInstruction<float>* init,
                            FIRBlockInstruction<float>* compute_control,
                            FIRBlockInstruction<float>* compute_dsp)
    :fName(name),
    fVersion(version_num),
    fNumInputs(inputs),
    fNumOutputs(ouputs),
    fIntHeapSize(int_heap_size),
    fRealHeapSize(real_heap_size),
    fSROffset(sr_offset),
    fCountOffset(count_offset),
    fUserInterfaceBlock(interface),
    fInitBlock(init),
    fComputeBlock(compute_control),
    fComputeDSPBlock(compute_dsp)
    {}
    
    virtual ~interpreter_dsp_factory()
    {
        // No more DSP instances, so delete
        delete fUserInterfaceBlock;
        delete fInitBlock;
        delete fComputeBlock;
        delete fComputeDSPBlock;
    }
    
    /* Return Factory name */
    std::string getName();
    
    /* Return Factory SHA key */
    std::string getSHAKey();
    
    /* Return Factory expanded DSP code */
    std::string getDSPCode();
    
    interpreter_dsp* createDSPInstance();
    
    void write(std::ostream* out);
    
    static interpreter_dsp_factory* read(std::istream* in);
    
    static FIRUserInterfaceBlockInstruction<float>* readUIBlock(std::istream* in);
    
    static FIRUserInterfaceInstruction<float>* readUIInstruction(std::stringstream* inst);
    
    static FIRBlockInstruction<float>* readCodeBlock(std::istream* in);
    
    static FIRBasicInstruction<float>* readCodeInstruction(std::istream* inst, std::istream* in);
    
};

template <class T>
class interpreter_dsp_aux : public dsp, public FIRInterpreter<T> {
	
    protected:
    
        interpreter_dsp_factory* fFactory;
  	
    public:
    
        interpreter_dsp_aux(interpreter_dsp_factory* factory)
        : FIRInterpreter<T>(factory->fIntHeapSize, factory->fRealHeapSize, factory->fSROffset, factory->fCountOffset)
        {
            fFactory = factory;
            this->fInputs = new FAUSTFLOAT*[fFactory->fNumInputs];
            this->fOutputs = new FAUSTFLOAT*[fFactory->fNumOutputs];
        }
    
        virtual ~interpreter_dsp_aux()
        {
            delete [] this->fInputs;
            delete [] this->fOutputs;
        }
          
        void static metadata(Meta* m) 
        {}

        virtual int getNumInputs() 
        {
            return fFactory->fNumInputs;
        }
        
        virtual int getNumOutputs() 
        {
            return fFactory->fNumOutputs;
        }
        
        virtual int getInputRate(int channel) 
        {
            return -1;
        }
        
        virtual int getOutputRate(int channel) 
        {
            return -1;
        }
        
        static void classInit(int samplingRate)
        {}
        
        virtual void instanceInit(int samplingRate)
        {
            // Store samplingRate in "fSamplingFreq" variable at correct offset in fIntHeap
            this->fIntHeap[this->fSROffset] = samplingRate;
            
            // Execute init instructions
            this->ExecuteBlock(fFactory->fInitBlock);
         }
        
        virtual void init(int samplingFreq) 
        {
            classInit(samplingFreq);
            instanceInit(samplingFreq);
        }
        
        virtual void buildUserInterface(UI* interface) 
        {
            this->ExecuteBuildUserInterface(fFactory->fUserInterfaceBlock, interface);
        }
        
        virtual void compute(int count, FAUSTFLOAT** inputs, FAUSTFLOAT** outputs) 
        {
            // Prepare in/out buffers
            for (int i = 0; i < fFactory->fNumInputs; i++) {
                this->fInputs[i] = inputs[i];
            }
            for (int i = 0; i < fFactory->fNumOutputs; i++) {
                this->fOutputs[i] = outputs[i];
            }
            
            // Executes the 'control' block
            this->ExecuteBlock(fFactory->fComputeBlock);
            
            // Executes the DSP loop
            this->ExecuteComputeBlock(fFactory->fComputeDSPBlock, count);
            
            //std::cout << "sample " << outputs[0][0] << std::endl;
       }
	
};

class EXPORT interpreter_dsp : public dsp {
                
    public:
    
        void metadata(Meta* m);
     
        int getNumInputs();
        int getNumOutputs();
    
        void init(int samplingRate);
        void instanceInit(int samplingRate);
      
        void buildUserInterface(UI* ui_interface);
        
        void compute(int count, FAUSTFLOAT** input, FAUSTFLOAT** output);
        
        interpreter_dsp* copy();
     
};

// Public C++ interface

EXPORT interpreter_dsp_factory* getDSPInterpreterFactoryFromSHAKey(const std::string& sha_key);

EXPORT interpreter_dsp_factory* createDSPInterpreterFactoryFromFile(const std::string& filename, 
                                                                  int argc, const char* argv[], 
                                                                  std::string& error_msg);

EXPORT interpreter_dsp_factory* createDSPInterpreterFactoryFromString(const std::string& name_app,
                                                                    const std::string& dsp_content,
                                                                    int argc, const char* argv[], 
                                                                    std::string& error_msg);

EXPORT bool deleteDSPInterpreterFactory(interpreter_dsp_factory* factory);

EXPORT std::vector<std::string> getDSPInterpreterFactoryLibraryList(interpreter_dsp_factory* factory);

EXPORT std::vector<std::string> getAllDSPInterpreterFactories();

EXPORT interpreter_dsp_factory* readDSPInterpreterFactoryFromMachine(const std::string& machine_code);

EXPORT std::string writeDSPInterpreterFactoryToMachine(interpreter_dsp_factory* factory);

EXPORT interpreter_dsp_factory* readDSPInterpreterFactoryFromMachineFile(const std::string& machine_code_path);

EXPORT void writeDSPInterpreterFactoryToMachineFile(interpreter_dsp_factory* factory, const std::string& machine_code_path);

EXPORT void deleteAllDSPInterpreterFactories();

EXPORT interpreter_dsp* createDSPInterpreterInstance(interpreter_dsp_factory* factory);

EXPORT void deleteDSPInterpreterInstance(interpreter_dsp* dsp);

#endif
