import numpy as np
import time

from bokeh.layouts import row, widgetbox
from bokeh.models import ColumnDataSource, CustomJS
from bokeh.models.widgets import Slider, TextInput, RadioButtonGroup, Toggle
from bokeh.plotting import figure

from .shepherd_io import ShepherdRawIO

shepherd_io = None
gui_doc = None

# Set up data
N = 1000
x = np.linspace(-10, 0, N)
adc_buffer = np.zeros((1000, 4))
idx = 0

source = ColumnDataSource(
    data=dict(
        x=x,
        v_in=adc_buffer[:, 0],
        i_in=adc_buffer[:, 1],
        v_out=adc_buffer[:, 2],
        i_out=adc_buffer[:, 3])
)

# These sources are just used to trigger callbacks on 'mouse up' events
# https://stackoverflow.com/questions/38375961/throttling-in-bokeh-application
dummy_source_dac0 = ColumnDataSource(data=dict(value=[]))
dummy_source_dac1 = ColumnDataSource(data=dict(value=[]))

# Set up plot
plot = figure(plot_height=800, plot_width=800, title="ADC readings",
              tools="crosshair,pan,reset,save,wheel_zoom",
              x_range=[-10, 0], y_range=[0, 2**18-1])

plot.line(
    'x', 'v_in', source=source, line_width=3, line_alpha=0.6, color='blue',
    legend='v in'
)
plot.line(
    'x', 'i_in', source=source, line_width=3, line_alpha=0.6, color='red',
    legend='i in'
)
plot.line(
    'x', 'v_out', source=source, line_width=3, line_alpha=0.6, color='green',
    legend='v out'
)
plot.line(
    'x', 'i_out', source=source, line_width=3, line_alpha=0.6, color='yellow',
    legend='i out'
)

liveplot_callback = None

# Set up widgets
liveplot_button = Toggle(label="Live plot", active=False)
dac0_slider = Slider(
    title="DAC0", value=0, start=0, end=2**18-1, step=250,
    callback_policy='mouseup'
)
dac1_slider = Slider(
    title="DAC1", value=0, start=0, end=2**18-1, step=250,
    callback_policy='mouseup'
)
power_button = Toggle(label="Power enable", active=False)
mppt_button = Toggle(label="MPPT enable", active=False)
hrvst_button = Toggle(label="Harvester enable", active=False)
lvlcnv_button = Toggle(label="Level converter enable", active=False)
vfix_button = Toggle(label="V_fixed enable", active=False)

power_button = Toggle(label="Power enable", active=True)
mode_radio_btns = RadioButtonGroup(labels=['Recording', 'Emulation'], active=0)
load_radio_btns = RadioButtonGroup(
    labels=['Artificial', 'Sensor Node'],
    active=0
)


# Set up callbacks
def change_power_state(attrname, old, new):
    shepherd_io.set_power(new)


# Set up callbacks
def change_mppt(attrname, old, new):
    shepherd_io.set_mppt(new)


# Set up callbacks
def change_vfix(attrname, old, new):
    shepherd_io.set_v_fixed(new)


# Set up callbacks
def change_hrvst(attrname, old, new):
    shepherd_io.set_harvester(new)


def change_lvlcnv(attrname, old, new):
    shepherd_io.set_lvl_conv(new)


def change_load(attrname, old, new):
    if new == 0:
        shepherd_io.set_load('artificial')
    elif new == 1:
        shepherd_io.set_load('node')


def change_timer(attrname, old, new):
    global liveplot_callback
    if(new is True):
        if liveplot_callback is None:
            liveplot_callback = gui_doc.add_periodic_callback(
                update_adc_plot, 10)
    else:
        if liveplot_callback is not None:
            gui_doc.remove_periodic_callback(liveplot_callback)
            liveplot_callback = None


# Set up callbacks
def update_dac0(attrname, old, new):
    shepherd_io.dac_write(0, dummy_source_dac0.data['value'][0])


# Set up callbacks
def update_dac1(attrname, old, new):
    shepherd_io.dac_write(1, dummy_source_dac1.data['value'][0])


def update_adc_plot():
    global idx
    global adc_buffer

    adc_buffer[idx, 0] = shepherd_io.adc_read(0)
    adc_buffer[idx, 1] = shepherd_io.adc_read(2)
    adc_buffer[idx, 2] = shepherd_io.adc_read(1)
    adc_buffer[idx, 3] = shepherd_io.adc_read(3)

    idx = (idx + 1) % N
    source.data = dict(
        x=x,
        v_in=np.roll(adc_buffer[:, 0], (N-1)-idx),
        i_in=np.roll(adc_buffer[:, 1], (N-1)-idx),
        v_out=np.roll(adc_buffer[:, 2], (N-1)-idx),
        i_out=np.roll(adc_buffer[:, 3], (N-1)-idx)
    )


dac0_slider.callback = CustomJS(
    args=dict(source=dummy_source_dac0),
    code='source.data = { value: [cb_obj.value] }'
)
dac1_slider.callback = CustomJS(
    args=dict(source=dummy_source_dac1),
    code='source.data = { value: [cb_obj.value] }'
)

dummy_source_dac0.on_change('data', update_dac0)
dummy_source_dac1.on_change('data', update_dac1)

load_radio_btns.on_change('active', change_load)

power_button.on_change('active', change_power_state)
hrvst_button.on_change('active', change_hrvst)
lvlcnv_button.on_change('active', change_lvlcnv)
mppt_button.on_change('active', change_mppt)
vfix_button.on_change('active', change_vfix)

liveplot_button.on_change('active', change_timer)

# Set up layouts and add to document
inputs = widgetbox(
    liveplot_button,
    dac0_slider,
    dac1_slider,
    load_radio_btns,
    hrvst_button,
    mppt_button,
    vfix_button,
    lvlcnv_button,
    power_button
)


def make_document(doc):
    global gui_doc
    global shepherd_io

    gui_doc = doc
    shepherd_io = ShepherdRawIO()
    shepherd_io.__enter__()
    doc.on_session_destroyed(shepherd_io.__exit__)
    doc.add_root(row(inputs, plot, width=800))
    doc.add_root(dummy_source_dac0)
    doc.add_root(dummy_source_dac1)
    doc.title = "SHEPHERD Analog Frontend Control"
