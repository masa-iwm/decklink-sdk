
#include "platform.h"

// Print out a DeckLink device's model and ID (if supported)
void PrintDeckLinkDetails (IDeckLink* deckLink)
{
    STRINGOBJ               deckLinkModelNameString = NULL;
    HRESULT                 result = E_FAIL;
    INT64_SIGNED            deckLinkPersistentId = 0;
    std::string             deviceName;
    IDeckLinkAttributes*    deckLinkAttributes = NULL;
    
    result = deckLink->GetModelName(&deckLinkModelNameString);

    if (result != S_OK)
    {
        fprintf(stderr, "\nDeckLink model information unavailable. Error: %08x", result);
        return;
    }
			
    // Obtain the deckLink device model name
    StringToStdString(deckLinkModelNameString, deviceName);
            
    printf("Model: %s ", deviceName.c_str());
    // Release the deckLink device model name string
    STRINGFREE(deckLinkModelNameString);
            
    result = deckLink->QueryInterface(IID_IDeckLinkAttributes, (void**)&deckLinkAttributes);
    if (result == S_OK)
    {
        // Obtain the deckLink device's unique id, if this feature is supported.
        result = deckLinkAttributes->GetInt(BMDDeckLinkPersistentID, &deckLinkPersistentId);
        if ((result == S_OK) && (deckLinkPersistentId > 0))
        {
            printf("Id: %lld", deckLinkPersistentId);
        }
        deckLinkAttributes->Release();
    }
}


// The device discovery callback class
class DeckLinkDeviceDiscoveryCallback : public IDeckLinkDeviceNotificationCallback
{
    
private:
    // A list of connected DeckLink devices.
    std::list<IDeckLink*>      deckLinkList;
    
    // A mutex is required...
    MUTEX	mutex;
    
public:
    
    DeckLinkDeviceDiscoveryCallback()
    {
        MutexInit(&mutex);
    }
    
    ~DeckLinkDeviceDiscoveryCallback(void)
    {
        // A mutex is not required, as UninstallNotifications() has been called guaranteeing that other callbacks will not occur.
        while (!deckLinkList.empty())
        {
            // Release any AddRef'ed DeckLink objects before the program exits.
            deckLinkList.back()->Release();
            deckLinkList.pop_back();
        }
        
        MutexDestroy(&mutex);
    }

	HRESULT     STDMETHODCALLTYPE DeckLinkDeviceArrived (/* in */ IDeckLink* deckLink)
	{
        printf("\nDeckLink device arrived. (%p) ", deckLink);
        
        MutexLock(&mutex);
            PrintDeckLinkDetails(deckLink);
        
            // The IDeckLink object must be kept so that device removal notifications can be received.
            deckLink->AddRef();
            deckLinkList.push_back(deckLink);
        MutexUnlock(&mutex);
        
        printf("\n\n");
        return S_OK;
	}
	
	HRESULT     STDMETHODCALLTYPE DeckLinkDeviceRemoved (/* in */ IDeckLink* deckLink)
	{
        std::list<IDeckLink*>::iterator         elementToRemove;
        
        printf("\nDeckLink device removed. (%p)", deckLink);
        
        MutexLock(&mutex);

            // locate DeckLink object in list and erase it
            elementToRemove = std::find(deckLinkList.begin(), deckLinkList.end(), deckLink);
            if (elementToRemove != deckLinkList.end())
                deckLinkList.erase(elementToRemove);

            // Release the reference on the IDeckLink instance we took in DeckLinkDeviceArrived()
            deckLink->Release();

        MutexUnlock(&mutex);
        
        printf("\n\n");
        return S_OK;
	}
		
	HRESULT		STDMETHODCALLTYPE QueryInterface (REFIID iid, LPVOID *ppv)
	{
        return E_NOINTERFACE;
    }
	
    ULONG		STDMETHODCALLTYPE AddRef ()
    {
        return 1;
    }
	
    ULONG		STDMETHODCALLTYPE Release ()
    {
        return 1;
    }
    
};


int		main (int argc, char** argv)
{
	IDeckLinkDiscovery*                   deckLinkDiscovery = NULL;
    DeckLinkDeviceDiscoveryCallback*      deckLinkDeviceDiscoveryCallback = NULL;
    
	HRESULT						result;
    INT8_UNSIGNED               returnCode = 1;

    Initialize();
    
    result = GetDeckLinkDiscoveryInstance(&deckLinkDiscovery);
    
    if((result != S_OK) || (deckLinkDiscovery == NULL))
	{
        fprintf(stderr, "Could not get DeckLink discovery instance. The DeckLink drivers may not be installed.\n");
		goto bail;
	}
	
    // Create an instance of notification callback
    deckLinkDeviceDiscoveryCallback = new DeckLinkDeviceDiscoveryCallback();

    if (deckLinkDeviceDiscoveryCallback == NULL)
    {
		fprintf(stderr, "Could not create device discovery callback object\n");
        goto bail;
    }
    
    // Set the device arrival / removal callback object
	if(deckLinkDiscovery->InstallDeviceNotifications(deckLinkDeviceDiscoveryCallback) != S_OK)
	{
		fprintf(stderr, "Could not install device discovery callback object\n");
        goto bail;	
	}
    
    printf("Waiting for DeckLink devices.... Press <RETURN> to exit\n");

    getchar();
    
    printf("Exiting.\n");

    // Uninstall device notifications
	if(deckLinkDiscovery->UninstallDeviceNotifications() != S_OK)
	{
		fprintf(stderr, "Could not uninstall device discovery callback objects\n");
        goto bail;	
	}
	
    // return success
    returnCode = 0;         
    
	// Release resources
bail:

    // Release the DeckLink discovery object
    if(deckLinkDiscovery != NULL)
        deckLinkDiscovery->Release();

    // Release the DeckLink device notification callback object
    if(deckLinkDeviceDiscoveryCallback)
        delete deckLinkDeviceDiscoveryCallback;
    
	return returnCode;
}
