// Include GLGizmoBase.hpp before I18N.hpp as it includes some libigl code, which overrides our localization "L" macro.
#include "GLGizmoCut.hpp"

#include <GL/glew.h>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/stattext.h>
#include <wx/sizer.h>

#include "slic3r/GUI/GUI_App.hpp"


namespace Slic3r {
namespace GUI {






// GLGizmoCut

class GLGizmoCutPanel : public wxPanel
{
public:
    GLGizmoCutPanel(wxWindow *parent);

    void display(bool display);
private:
    bool m_active;
    wxCheckBox *m_cb_rotate;
    wxButton *m_btn_cut;
    wxButton *m_btn_cancel;
};

GLGizmoCutPanel::GLGizmoCutPanel(wxWindow *parent)
    : wxPanel(parent)
    , m_active(false)
    , m_cb_rotate(new wxCheckBox(this, wxID_ANY, _(L("Rotate lower part upwards"))))
    , m_btn_cut(new wxButton(this, wxID_OK, _(L("Perform cut"))))
    , m_btn_cancel(new wxButton(this, wxID_CANCEL, _(L("Cancel"))))
{
    enum { MARGIN = 5 };

    auto *sizer = new wxBoxSizer(wxHORIZONTAL);

    auto *label = new wxStaticText(this, wxID_ANY, _(L("Cut object:")));
    sizer->Add(label, 0, wxALL | wxALIGN_CENTER, MARGIN);
    sizer->Add(m_cb_rotate, 0, wxALL | wxALIGN_CENTER, MARGIN);
    sizer->AddStretchSpacer();
    sizer->Add(m_btn_cut, 0, wxALL | wxALIGN_CENTER, MARGIN);
    sizer->Add(m_btn_cancel, 0, wxALL | wxALIGN_CENTER, MARGIN);

    SetSizer(sizer);
}

void GLGizmoCutPanel::display(bool display)
{
    Show(display);
    GetParent()->Layout();
}


const double GLGizmoCut::Offset = 10.0;
const double GLGizmoCut::Margin = 20.0;
const std::array<float, 3> GLGizmoCut::GrabberColor = { 1.0, 0.5, 0.0 };

#if ENABLE_SVG_ICONS
GLGizmoCut::GLGizmoCut(GLCanvas3D& parent, const std::string& icon_filename, unsigned int sprite_id)
    : GLGizmoBase(parent, icon_filename, sprite_id)
#else
GLGizmoCut::GLGizmoCut(GLCanvas3D& parent, unsigned int sprite_id)
    : GLGizmoBase(parent, sprite_id)
#endif // ENABLE_SVG_ICONS
    , m_cut_z(0.0)
    , m_max_z(0.0)
#if !ENABLE_IMGUI
    , m_panel(nullptr)
#endif // not ENABLE_IMGUI
    , m_keep_upper(true)
    , m_keep_lower(true)
    , m_rotate_lower(false)
{}

#if !ENABLE_IMGUI
void GLGizmoCut::create_external_gizmo_widgets(wxWindow *parent)
{
    wxASSERT(m_panel == nullptr);

    m_panel = new GLGizmoCutPanel(parent);
    parent->GetSizer()->Add(m_panel, 0, wxEXPAND);

    parent->Layout();
    parent->Fit();
    auto prev_heigh = parent->GetMinSize().GetHeight();
    parent->SetMinSize(wxSize(-1, std::max(prev_heigh, m_panel->GetSize().GetHeight())));

    m_panel->Hide();
    m_panel->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        perform_cut(m_parent.get_selection());
    }, wxID_OK);
}
#endif // not ENABLE_IMGUI

bool GLGizmoCut::on_init()
{
    m_grabbers.emplace_back();
    m_shortcut_key = WXK_CONTROL_C;
    return true;
}

std::string GLGizmoCut::on_get_name() const
{
    return L("Cut [C]");
}

void GLGizmoCut::on_set_state()
{
    // Reset m_cut_z on gizmo activation
    if (get_state() == On) {
        m_cut_z = m_parent.get_selection().get_bounding_box().size()(2) / 2.0;
    }

#if !ENABLE_IMGUI
    // Display or hide the extra panel
    if (m_panel != nullptr) {
        m_panel->display(get_state() == On);
    }
#endif // not ENABLE_IMGUI
}

bool GLGizmoCut::on_is_activable(const GLCanvas3D::Selection& selection) const
{
    return selection.is_single_full_instance() && !selection.is_wipe_tower();
}

void GLGizmoCut::on_start_dragging(const GLCanvas3D::Selection& selection)
{
    if (m_hover_id == -1) { return; }

    const BoundingBoxf3& box = selection.get_bounding_box();
    m_start_z = m_cut_z;
    update_max_z(selection);
    m_drag_pos = m_grabbers[m_hover_id].center;
    m_drag_center = box.center();
    m_drag_center(2) = m_cut_z;
}

void GLGizmoCut::on_update(const UpdateData& data, const GLCanvas3D::Selection& selection)
{
    if (m_hover_id != -1) {
        set_cut_z(m_start_z + calc_projection(data.mouse_ray));
    }
}

void GLGizmoCut::on_render(const GLCanvas3D::Selection& selection) const
{
    if (m_grabbers[0].dragging) {
        set_tooltip("Z: " + format(m_cut_z, 2));
    }

    update_max_z(selection);

    const BoundingBoxf3& box = selection.get_bounding_box();
    Vec3d plane_center = box.center();
    plane_center(2) = m_cut_z;

    const float min_x = box.min(0) - Margin;
    const float max_x = box.max(0) + Margin;
    const float min_y = box.min(1) - Margin;
    const float max_y = box.max(1) + Margin;
    ::glEnable(GL_DEPTH_TEST);
    ::glDisable(GL_CULL_FACE);
    ::glEnable(GL_BLEND);
    ::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Draw the cutting plane
    ::glBegin(GL_QUADS);
    ::glColor4f(0.8f, 0.8f, 0.8f, 0.5f);
    ::glVertex3f(min_x, min_y, plane_center(2));
    ::glVertex3f(max_x, min_y, plane_center(2));
    ::glVertex3f(max_x, max_y, plane_center(2));
    ::glVertex3f(min_x, max_y, plane_center(2));
    ::glEnd();

    ::glEnable(GL_CULL_FACE);
    ::glDisable(GL_BLEND);

    // TODO: draw cut part contour?

    // Draw the grabber and the connecting line
    m_grabbers[0].center = plane_center;
    m_grabbers[0].center(2) = plane_center(2) + Offset;

    ::glDisable(GL_DEPTH_TEST);
    ::glLineWidth(m_hover_id != -1 ? 2.0f : 1.5f);
    ::glColor3f(1.0, 1.0, 0.0);
    ::glBegin(GL_LINES);
    ::glVertex3dv(plane_center.data());
    ::glVertex3dv(m_grabbers[0].center.data());
    ::glEnd();

    std::copy(std::begin(GrabberColor), std::end(GrabberColor), m_grabbers[0].color);
    m_grabbers[0].render(m_hover_id == 0, box.max_size());
}

void GLGizmoCut::on_render_for_picking(const GLCanvas3D::Selection& selection) const
{
    ::glDisable(GL_DEPTH_TEST);

    render_grabbers_for_picking(selection.get_bounding_box());
}

#if ENABLE_IMGUI
void GLGizmoCut::on_render_input_window(float x, float y, float bottom_limit, const GLCanvas3D::Selection& selection)
{
    m_imgui->set_next_window_pos(x, y, ImGuiCond_Always);
    m_imgui->set_next_window_bg_alpha(0.5f);
    m_imgui->begin(_(L("Cut")), ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::PushItemWidth(100.0f);
    bool _value_changed = ImGui::InputDouble("Z", &m_cut_z, 0.0f, 0.0f, "%.2f");

    m_imgui->checkbox(_(L("Keep upper part")), m_keep_upper);
    m_imgui->checkbox(_(L("Keep lower part")), m_keep_lower);
    m_imgui->checkbox(_(L("Rotate lower part upwards")), m_rotate_lower);

    m_imgui->disabled_begin(!m_keep_upper && !m_keep_lower);
    const bool cut_clicked = m_imgui->button(_(L("Perform cut")));
    m_imgui->disabled_end();

    m_imgui->end();

    if (cut_clicked && (m_keep_upper || m_keep_lower)) {
        perform_cut(selection);
    }
}
#endif // ENABLE_IMGUI

void GLGizmoCut::update_max_z(const GLCanvas3D::Selection& selection) const
{
    m_max_z = selection.get_bounding_box().size()(2);
    set_cut_z(m_cut_z);
}

void GLGizmoCut::set_cut_z(double cut_z) const
{
    // Clamp the plane to the object's bounding box
    m_cut_z = std::max(0.0, std::min(m_max_z, cut_z));
}

void GLGizmoCut::perform_cut(const GLCanvas3D::Selection& selection)
{
    const auto instance_idx = selection.get_instance_idx();
    const auto object_idx = selection.get_object_idx();

    wxCHECK_RET(instance_idx >= 0 && object_idx >= 0, "GLGizmoCut: Invalid object selection");

    wxGetApp().plater()->cut(object_idx, instance_idx, m_cut_z, m_keep_upper, m_keep_lower, m_rotate_lower);
}

double GLGizmoCut::calc_projection(const Linef3& mouse_ray) const
{
    double projection = 0.0;

    const Vec3d starting_vec = m_drag_pos - m_drag_center;
    const double len_starting_vec = starting_vec.norm();
    if (len_starting_vec != 0.0)
    {
        Vec3d mouse_dir = mouse_ray.unit_vector();
        // finds the intersection of the mouse ray with the plane parallel to the camera viewport and passing throught the starting position
        // use ray-plane intersection see i.e. https://en.wikipedia.org/wiki/Line%E2%80%93plane_intersection algebric form
        // in our case plane normal and ray direction are the same (orthogonal view)
        // when moving to perspective camera the negative z unit axis of the camera needs to be transformed in world space and used as plane normal
        Vec3d inters = mouse_ray.a + (m_drag_pos - mouse_ray.a).dot(mouse_dir) / mouse_dir.squaredNorm() * mouse_dir;
        // vector from the starting position to the found intersection
        Vec3d inters_vec = inters - m_drag_pos;

        // finds projection of the vector along the staring direction
        projection = inters_vec.dot(starting_vec.normalized());
    }
    return projection;
}


} // namespace GUI
} // namespace Slic3r
