/*
Copyright (C) 2011 MoSync AB

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License,
version 2, as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
MA 02110-1301, USA.
*/

/**
 * @file WebViewLoveSMS.cpp
 * @author Mikael Kindborg
 *
 * Application for sending Love SMSs.
 *
 * This program illustrates how to use WebView for the
 * user interface of a MoSync C++ application.
 *
 * An application can divide the program code between the
 * WebView layer and the C++ layer in a variety of ways.
 * In one extreme, the WebView is used purely for
 * rendering the user interface, the HTML/CSS/JavaScript
 * code could even be generated by C++ code. In the other
 * extreme, almost the entire application is written in
 * HTML/CSS/JavaScript, and only the calls needed to access
 * native functionality via the MoSync API is written in C++.
 *
 * Which approach is chosen depends on the preferences of the
 * development team, existing code and libraries, compatibility
 * considerations etc. Some teams may prefer to be C++ centric,
 * white others may prefer to do most of the development using
 * JavScript and web technologies.
 *
 * In this application, much of the application logic is written
 * in JavaScript, and the C++ layer is used for sending text
 * messages and for saving/loading the phone number entered
 * in the user interface. There is only one phone number saved,
 * because, after all, this is an application to be used with
 * your loved one. ;-)
 */

#include <ma.h>				// MoSync API (base API).
#include <maheap.h>			// C memory allocation functions.
#include <mastring.h>		// C String functions.
#include <mavsprintf.h>		// sprintf etc.
#include <MAUtil/String.h>	// Class String.
#include <IX_WIDGET.h>		// Widget API.
#include <conprint.h>		// Debug logging, must build in debug mode
							// for log messages to display.
#include "MAHeaders.h"		// Resource identifiers, not a physical file.
#include "WebViewUtil.h"	// Classes Platform and WebViewMessage.

using namespace MoSync;

// Set to true to actually send SMS.
// You can turn off SMS sending during debugging
// by setting this variable to false.
static bool G_SendSMSForReal = true;

/**
 * The application class.
 */
class WebViewLoveSMSApp
{
private:
	MAWidgetHandle mScreen;
	MAWidgetHandle mWebView;
	Platform* mPlatform;
	MAUtil::String mLoveMessage;
	MAUtil::String mKissMessage;

public:
	WebViewLoveSMSApp()
	{
		mPlatform = Platform::create();
		createUI();
	}

	virtual ~WebViewLoveSMSApp()
	{
		destroyUI();
	}

	void createUI()
	{
		// Create the screen that will hold the web view.
		mScreen = maWidgetCreate(MAW_SCREEN);
		widgetShouldBeValid(mScreen, "Could not create screen");

		// Create the web view.
		mWebView = createWebView();

		// We need to copy the bundle with HTML/Media files to
		// the application's local storage directory. The WebView
		// will load the files from that location.
		mPlatform->extractLocalFiles();

		// Now we set the main page. This will load the page
		// we saved, which in turn will load the background icon.
		maWidgetSetProperty(mWebView, "url", "PageMain.html");

		// Add the web view to the screen.
		maWidgetAddChild(mScreen, mWebView);

		// Show the screen.
		maWidgetScreenShow(mScreen);
	}

	MAWidgetHandle createWebView()
	{
		// Create web view
		MAWidgetHandle webView = maWidgetCreate(MAW_WEB_VIEW);
		widgetShouldBeValid(webView, "Could not create web view");

		// Set size of web view to fill the parent.
		maWidgetSetProperty(webView, "width", "-1");
		maWidgetSetProperty(webView, "height", "-1");

		// Disable zooming to make page display in readable size
		// on all devices.
		maWidgetSetProperty(webView, "enableZoom", "false");

		// Register a handler for JavaScript messages.
		WebViewMessage::getMessagesFor(webView);

		return webView;
	}

	void destroyUI()
	{
		maWidgetDestroy(mScreen);
		delete mPlatform;
	}

	void runEventLoop()
	{
		MAEvent event;

		bool isRunning = true;
		while (isRunning)
		{
			maWait(0);
			maGetEvent(&event);
			switch (event.type)
			{
				case EVENT_TYPE_CLOSE:
					isRunning = false;
					break;

				case EVENT_TYPE_KEY_PRESSED:
					if (event.key == MAK_BACK)
					{
						isRunning = false;
					}
					break;

				case EVENT_TYPE_WIDGET:
					handleWidgetEvent((MAWidgetEventData*) event.data);
					break;

				case EVENT_TYPE_SMS:
					// Depending on the event status, we call
					// different JavaScript functions. These are
					// currently hard-coded, but could be passed
					// as parameters to decouple the JavaScript
					// code from the C++ code.
					if (MA_SMS_RESULT_SENT == event.status)
					{
						callJSFunction("SMSSent");
					}
					else if (MA_SMS_RESULT_NOT_SENT == event.status)
					{
						callJSFunction("SMSNotSent");
					}
					else if (MA_SMS_RESULT_DELIVERED == event.status)
					{
						callJSFunction("SMSDelivered");
					}
					else if (MA_SMS_RESULT_NOT_DELIVERED == event.status)
					{
						callJSFunction("SMSNotDelivered");
					}
					break;
			}
		}
	}

	void handleWidgetEvent(MAWidgetEventData* widgetEvent)
	{
		// Handle messages from the WebView widget.
		if (MAW_EVENT_WEB_VIEW_HOOK_INVOKED == widgetEvent->eventType &&
			MAW_CONSTANT_HARD == widgetEvent->hookType)
		{
			// Get message.
			WebViewMessage message(widgetEvent->urlData);

			if (message.is("SendSMS"))
			{
				// Save phone no and send SMS.
				savePhoneNoAndSendSMS(
					message.getParam(0),
					message.getParam(1));
			}
			else if (message.is("PageLoaded"))
			{
				// Load and set saved phone number.
				// We could implement a JavaScript File API to do
				// this, which would be a much more general way.
				setSavedPhoneNo();
			}
		}
	}

	void savePhoneNoAndSendSMS(
		const MAUtil::String& phoneNo,
		const MAUtil::String&  message)
	{
		lprintfln("*** SMS to: %s\n", phoneNo.c_str());
		lprintfln("*** SMS data: %s\n", message.c_str());

		// Save the phone number.
		savePhoneNo(phoneNo);

		if (G_SendSMSForReal)
		{
			// Send the message.
			int result = maSendTextSMS(
				phoneNo.c_str(),
				message.c_str());

			// Provide feedback via JS.
			if (0 != result)
			{
				callJSFunction("SMSNotSent");
			}
		}
		else
		{
			callJSFunction("SMSNotSent");
		}
	}

	/**
	 * Call a JavaScript function.
	 */
	void callJSFunction(const MAUtil::String& fun)
	{
		char code[512];
		sprintf(code, "%s()", fun.c_str());
		callJS(code);
	}

	/**
	 * Run JavaScript code.
	 */
	void callJS(const MAUtil::String& code)
	{
		char script[512];
		sprintf(script, "javascript:%s", code.c_str());
		maWidgetSetProperty(mWebView, "url", script);
	}

	/**
	 * Read saved phone number and set it on
	 * the JavaScript side.
	 */
	void setSavedPhoneNo()
	{
		char script[512];
		sprintf(
			script,
			"javascript:SetPhoneNo('%s')",
			loadPhoneNo().c_str());
		maWidgetSetProperty(mWebView, "url", script);
	}

	/**
	 * Save the phone number.
	 */
	void savePhoneNo(const MAUtil::String& phoneNo)
	{
		mPlatform->writeTextToFile(phoneNoPath(), phoneNo);
	}

	/**
	 * Load the phone number.
	 */
	MAUtil::String loadPhoneNo()
	{
		MAUtil::String phoneNo;
		bool success = mPlatform->readTextFromFile(phoneNoPath(), phoneNo);
		if (success)
		{
			return phoneNo;
		}
		else
		{
			return "";
		}
	}

	MAUtil::String phoneNoPath()
	{
		return mPlatform->getLocalPath() + "SavedPhoneNo";
	}

	void widgetShouldBeValid(MAWidgetHandle widget, const char* panicMessage)
	{
		if (widget <= 0)
		{
			maPanic(0, panicMessage);
		}
	}
};
// End of class WebViewLoveSMSApp

/**
 * Main function that is called when the program starts.
 */
extern "C" int MAMain()
{
	WebViewLoveSMSApp app;
	app.runEventLoop();
	return 0;
}
