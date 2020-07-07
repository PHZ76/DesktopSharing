#include "Overlay.h"
#include <mutex>

Overlay::Overlay()
{
	memset(live_info_, 0, sizeof(live_info_));
	memset(&rect_, 0, sizeof(SDL_Rect));

	snprintf(live_info_[RTSP_SERVER_EVENT].server_ip, sizeof(LiveInfo::server_ip), "%s", "127.0.0.1");
	snprintf(live_info_[RTSP_SERVER_EVENT].server_port, sizeof(LiveInfo::server_port), "%s", "8554");
	snprintf(live_info_[RTSP_SERVER_EVENT].server_stream, sizeof(LiveInfo::server_stream), "%s", "live");

	snprintf(live_info_[RTMP_PUSHER_EVENT].pusher_url, sizeof(LiveInfo::pusher_url), "%s", "rtmp://127.0.0.1:1935/live/test");
	snprintf(live_info_[RTSP_PUSHER_EVENT].pusher_url, sizeof(LiveInfo::pusher_url), "%s", "rtsp://127.0.0.1:554/live/test");
}

Overlay::~Overlay()
{

}

void Overlay::RegisterObserver(OverlayCallack* callback)
{
	callback_ = callback;
}

bool Overlay::Init(SDL_Window* window, void *device)
{
	static std::once_flag init_flag;
	std::call_once(init_flag, [=]() {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
		ImGui::GetStyle().AntiAliasedLines = false;
		ImGui::GetStyle().WindowRounding = 0;
	});

	if (device_ != nullptr) {
		Destroy();
	}

	if (device) {
		ImGui_ImplSDL2_InitForD3D(window);
		ImGui_ImplDX9_Init(reinterpret_cast<IDirect3DDevice9*>(device));
		window_ = window;
		device_ = (IDirect3DDevice9 *)device;
		return true;
	}

	return false;
}

void Overlay::SetRect(int x, int y, int w, int h)
{
	rect_.x = x;
	rect_.y = y;
	rect_.w = w;
	rect_.h = h;
}

void Overlay::Destroy()
{
	if (device_) {
		window_ = nullptr;
		device_ = nullptr;
		memset(&rect_, 0, sizeof(SDL_Rect));
		ImGui_ImplDX9_Shutdown();
		ImGui_ImplSDL2_Shutdown();
	}
}

bool Overlay::Begin()
{
	if (!device_ || !window_) {
		return false;
	}

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplSDL2_NewFrame(window_);
	ImGui::NewFrame();
	return true;
}

bool Overlay::End()
{
	if (!device_ || !window_) {
		return false;
	}

	ImGui::Render();
	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
	ImGui::EndFrame();
	return true;
}

bool Overlay::Render()
{
	if (!Begin()) {
		return false;
	}

	Copy();
	End();
	return true;
}

void Overlay::Process(SDL_Event* event)
{
	ImGui_ImplSDL2_ProcessEvent(event);
}

bool Overlay::Copy()
{
	if (!rect_.w || !rect_.h) {
		return false;
	}

	int widget_flag = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | 
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

	ImGui::SetNextWindowPos(ImVec2((float)rect_.x, (float)rect_.y), ImGuiCond_Always);
	//ImGui::SetNextWindowSize(ImVec2((float)rect_.w * 3 / 4, (float)rect_.h), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2((float)rect_.w, (float)rect_.h), ImGuiCond_Always);
	ImGui::Begin("screen-live-setting-widget", nullptr, widget_flag);

	int input_flag = 0;

	/* Encoder selection */
	int encoder_index = 0;
	ImGui::SetCursorPos(ImVec2((float)20, (float)20));
	ImGui::Text("Video Encoder: "); //ImGui::SameLine();
	ImGui::SetCursorPos(ImVec2((float)125, (float)17));
	ImGui::RadioButton("x264", &encoder_index_, 1); ImGui::SameLine(0, 10);
	ImGui::RadioButton("nvenc", &encoder_index_, 2);

	/* RTSP Server setting */
	LiveInfo& rtsp_server_info = live_info_[RTSP_SERVER_EVENT];
	ImGui::SetCursorPos(ImVec2((float)20, (float)50));
	ImGui::Text("RTSP Server:"); ImGui::SameLine();
	ImGui::Text(" ip:"); // ImGui::SameLine();
	ImGui::SetNextItemWidth(120);
	ImGui::SetCursorPos(ImVec2((float)145, (float)48));
	ImGui::InputText("##rtsp-server-ip", rtsp_server_info.server_ip, sizeof(LiveInfo::server_ip), input_flag); ImGui::SameLine(0, 10);
	ImGui::Text("port:"); ImGui::SameLine();
	ImGui::SetNextItemWidth(50);
	ImGui::InputText("##rtsp-server-port", rtsp_server_info.server_port, sizeof(LiveInfo::server_port), input_flag); ImGui::SameLine(0, 10);
	ImGui::Text("stream:"); ImGui::SameLine();
	ImGui::SetNextItemWidth(120);
	ImGui::InputText("##rtsp-server-stream", rtsp_server_info.server_stream, sizeof(LiveInfo::server_stream), input_flag); ImGui::SameLine(0, 20);
	if (ImGui::Button(!rtsp_server_info.state ? "start##rtsp-server" : "stop##rtsp-server", ImVec2(50.0, 20.0))) {
		rtsp_server_info.state = !rtsp_server_info.state;
		NotifyEvent(RTSP_SERVER_EVENT);
	}
	ImGui::SameLine(0, 10);
	ImGui::Text("%s", rtsp_server_info.state_info);

	/* RTSP Pusher setting */
	LiveInfo& rtsp_pusher_info = live_info_[RTSP_PUSHER_EVENT];
	ImGui::SetCursorPos(ImVec2((float)20, (float)80));
	ImGui::Text("RTSP Pusher:"); ImGui::SameLine();
	ImGui::Text("url:");  //ImGui::SameLine();
	ImGui::SetNextItemWidth(350);
	ImGui::SetCursorPos(ImVec2((float)145, (float)78));
	ImGui::InputText("##rtsp-pusher-url", rtsp_pusher_info.pusher_url, sizeof(LiveInfo::pusher_url), input_flag); ImGui::SameLine(0, 10);
	if (ImGui::Button(!rtsp_pusher_info.state ? "start##rtsp-pusher" : "stop##rtsp-pusher", ImVec2(50.0, 20.0))) {
		rtsp_pusher_info.state = !rtsp_pusher_info.state;
		NotifyEvent(RTSP_PUSHER_EVENT);
	}
	ImGui::SameLine(0, 10);
	ImGui::Text("%s", rtsp_pusher_info.state_info);

	/* RTMP Pusher setting */
	LiveInfo& rtmp_pusher_info = live_info_[RTMP_PUSHER_EVENT];
	ImGui::SetCursorPos(ImVec2((float)20, (float)110));
	ImGui::Text("RTMP Pusher:"); ImGui::SameLine();
	ImGui::Text("url:");  //ImGui::SameLine();
	ImGui::SetNextItemWidth(350);
	ImGui::SetCursorPos(ImVec2((float)145, (float)108));
	ImGui::InputText("##rtmp-pusher-url", rtmp_pusher_info.pusher_url, sizeof(LiveInfo::pusher_url), input_flag); ImGui::SameLine(0, 10);
	if (ImGui::Button(!rtmp_pusher_info.state ? "start##rtmp-pusher" : "stop##rtmp-pusher", ImVec2(50.0, 20.0))) {
		rtmp_pusher_info.state = !rtmp_pusher_info.state;
		NotifyEvent(RTMP_PUSHER_EVENT);
	} 
	ImGui::SameLine(0, 10);
	ImGui::Text("%s", rtmp_pusher_info.state_info);

	ImGui::End();

	//ImGui::SetNextWindowPos(ImVec2((float)rect_.x + (float)rect_.w * 3/4, (float)rect_.y), ImGuiCond_Always);
	//ImGui::SetNextWindowSize(ImVec2((float)rect_.w/4, (float)rect_.h), ImGuiCond_Always);
	//ImGui::Begin("screen-live-info-widget", nullptr, widget_flag);
	//ImGui::Text("%s", " ");
	//ImGui::End();

	return true;
}

void Overlay::SetLiveState(int event_type, bool state)
{
	live_info_[event_type].state = state;
	memset(live_info_[event_type].state_info, 0, sizeof(LiveInfo::state_info));
}

void Overlay::NotifyEvent(int event_type)
{	
	bool *state = nullptr;
	int live_index = -1;
	std::vector<std::string> settings(2);

	if (!callback_) {
		return;
	}

	settings[0] = std::string("x264");
	if(encoder_index_ == 2) {
		settings[0] = std::string("nvenc");
	}

	switch (event_type)
	{
		case RTSP_SERVER_EVENT:
		{
			live_index = RTSP_SERVER_EVENT;
			LiveInfo& rtsp_server_info = live_info_[live_index];
			settings.resize(4);
			settings[1] = std::string(rtsp_server_info.server_ip);
			settings[2] = std::string(rtsp_server_info.server_port);
			settings[3] = std::string(rtsp_server_info.server_stream);
			state = &rtsp_server_info.state;
		}
		break;

		case RTSP_PUSHER_EVENT:
		{
			live_index = RTSP_PUSHER_EVENT;
			LiveInfo& rtsp_pusher_info = live_info_[live_index];
			settings[1] = std::string(rtsp_pusher_info.pusher_url);
			state = &rtsp_pusher_info.state;
		}
		break;

		case RTMP_PUSHER_EVENT:
		{
			live_index = RTMP_PUSHER_EVENT;
			LiveInfo& rtmp_pusher_info = live_info_[live_index];
			settings[1] = std::string(rtmp_pusher_info.pusher_url);
			state = &rtmp_pusher_info.state;
		}
		break;

		default:
		{

		}
		break;
	}
	
	if (callback_ && live_index>=0) {
		if (*state) {
			*state = callback_->StartLive(event_type, settings);
			if (!*state) {
				snprintf(live_info_[live_index].state_info, sizeof(LiveInfo::state_info), "%s", "failed");
			}
			//else {
				//snprintf(live_info_[live_index].state_info, sizeof(LiveInfo::state_info), "%s", "succeeded");
			//}
		}
		else {
			callback_->StopLive(event_type);
			memset(live_info_[live_index].state_info, 0, sizeof(LiveInfo::state_info));
		}
	}
}