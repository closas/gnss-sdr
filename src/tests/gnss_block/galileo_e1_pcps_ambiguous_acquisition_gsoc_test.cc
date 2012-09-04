/*!
 * \file galileo_e1_pcps_ambiguous_acquisition_gsoc_test.cc
 * \brief  This class implements an acquisition test based on GSoC 2012
 *         experiments.
 *
 * This test is a part of an experiment performed by Luis Esteve in the
 * framework of the Google Summer of Code (GSoC) 2012, with the collaboration
 * of Javier Arribas and Carles Fernández, related to the extension of GNSS-SDR
 *  to Galileo. The objective is perform a positive acquisition of in-orbit
 *  Galileo signals in the E1 band.
 *
 * \author Luis Esteve, 2012. luis(at)epsilon-formacion.com
 *
 *
 * -------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2012  (see AUTHORS file for a list of contributors)
 *
 * GNSS-SDR is a software defined Global Navigation
 *          Satellite Systems receiver
 *
 * This file is part of GNSS-SDR.
 *
 * GNSS-SDR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * at your option) any later version.
 *
 * GNSS-SDR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNSS-SDR. If not, see <http://www.gnu.org/licenses/>.
 *
 * -------------------------------------------------------------------------
 */



#include <gtest/gtest.h>
#include <sys/time.h>
#include <iostream>
#include <gnuradio/gr_top_block.h>
#include <gnuradio/gr_file_source.h>
#include <gnuradio/gr_sig_source_c.h>
#include <gnuradio/gr_msg_queue.h>
#include <gnuradio/gr_null_sink.h>
#include <gnuradio/gr_skiphead.h>

#include "gnss_block_factory.h"
#include "gnss_block_interface.h"
#include "in_memory_configuration.h"
#include "gnss_sdr_valve.h"
#include "gnss_signal.h"
#include "gnss_synchro.h"

#include "galileo_e1_pcps_ambiguous_acquisition.h"

class GalileoE1PcpsAmbiguousAcquisitionGSoCTest: public ::testing::Test {
protected:
	GalileoE1PcpsAmbiguousAcquisitionGSoCTest() {
		queue = gr_make_msg_queue(0);
		top_block = gr_make_top_block("Acquisition test");
		factory = new GNSSBlockFactory();
		config = new InMemoryConfiguration();
		item_size = sizeof(gr_complex);
		stop = false;
		message = 0;
	}

	~GalileoE1PcpsAmbiguousAcquisitionGSoCTest() {
		delete factory;
		delete config;
	}

	void init();
	void start_queue();
	void wait_message();
	void stop_queue();

	gr_msg_queue_sptr queue;
	gr_top_block_sptr top_block;
	GNSSBlockFactory* factory;
	InMemoryConfiguration* config;
	Gnss_Synchro gnss_synchro;
	size_t item_size;
    concurrent_queue<int> channel_internal_queue;
    bool stop;
    int message;
    boost::thread ch_thread;
};

void GalileoE1PcpsAmbiguousAcquisitionGSoCTest::init(){

    gnss_synchro.Channel_ID=0;
    gnss_synchro.System = 'E';
    std::string signal = "1C";
    signal.copy(gnss_synchro.Signal,2,0);
    gnss_synchro.PRN=11;

    config->set_property("GNSS-SDR.internal_fs_hz", "4000000");

	config->set_property("Acquisition.item_type", "gr_complex");
	config->set_property("Acquisition.if", "0");
	config->set_property("Acquisition.sampled_ms", "4");
	config->set_property("Acquisition.dump", "false");
	config->set_property("Acquisition.implementation", "Galileo_E1_PCPS_Ambiguous_Acquisition");
	config->set_property("Acquisition.threshold", "50");
	config->set_property("Acquisition.doppler_max", "10000");
	config->set_property("Acquisition.doppler_step", "125");
	config->set_property("Acquisition.repeat_satellite", "false");
	config->set_property("Acquisition0.cboc", "true");
}

void GalileoE1PcpsAmbiguousAcquisitionGSoCTest::start_queue()
{
    ch_thread = boost::thread(&GalileoE1PcpsAmbiguousAcquisitionGSoCTest::wait_message, this);
}


void GalileoE1PcpsAmbiguousAcquisitionGSoCTest::wait_message()
{
    while (!stop)
        {
            channel_internal_queue.wait_and_pop(message);
            stop_queue();
        }
}

void GalileoE1PcpsAmbiguousAcquisitionGSoCTest::stop_queue()
{
	stop = true;
 }



TEST_F(GalileoE1PcpsAmbiguousAcquisitionGSoCTest, Instantiate)
{

	init();

	GalileoE1PcpsAmbiguousAcquisition *acquisition = new GalileoE1PcpsAmbiguousAcquisition(config, "Acquisition", 1, 1, queue);
    EXPECT_STREQ("Galileo_E1_PCPS_Ambiguous_Acquisition", acquisition->implementation().c_str());

	delete acquisition;

}

TEST_F(GalileoE1PcpsAmbiguousAcquisitionGSoCTest, ConnectAndRun)
{
	int fs_in = 4000000;
	int nsamples = 4*fs_in;
	struct timeval tv;
    long long int begin = 0;
    long long int end = 0;

    init();
	GalileoE1PcpsAmbiguousAcquisition *acquisition = new GalileoE1PcpsAmbiguousAcquisition(config, "Acquisition", 1, 1, queue);

	ASSERT_NO_THROW( {
			acquisition->connect(top_block);

			gr_sig_source_c_sptr source = gr_make_sig_source_c(fs_in,GR_SIN_WAVE, 1000, 1, gr_complex(0));
			gr_block_sptr valve = gnss_sdr_make_valve(sizeof(gr_complex), nsamples, queue);

			top_block->connect(source, 0, valve, 0);
			top_block->connect(valve, 0, acquisition->get_left_block(), 0);
		}) << "Failure connecting the blocks of acquisition test."<< std::endl;

	EXPECT_NO_THROW( {
			gettimeofday(&tv, NULL);
			begin = tv.tv_sec *1000000 + tv.tv_usec;
			top_block->run(); // Start threads and wait
			gettimeofday(&tv, NULL);
			end = tv.tv_sec *1000000 + tv.tv_usec;
		}) << "Failure running he top_block."<< std::endl;

	delete acquisition;
	std::cout <<  "Processed " << nsamples << " samples in " << (end-begin) << " microseconds" << std::endl;

}

TEST_F(GalileoE1PcpsAmbiguousAcquisitionGSoCTest, ValidationOfResults)
{
	struct timeval tv;
    long long int begin = 0;
    long long int end = 0;

    init();
	GalileoE1PcpsAmbiguousAcquisition *acquisition = new GalileoE1PcpsAmbiguousAcquisition(config, "Acquisition", 1, 1, queue);


	ASSERT_NO_THROW( {
	    	acquisition->set_channel(gnss_synchro.Channel_ID);
	}) << "Failure setting channel."<< std::endl;

	ASSERT_NO_THROW( {
	acquisition->set_gnss_synchro(&gnss_synchro);
	}) << "Failure setting gnss_synchro."<< std::endl;

	ASSERT_NO_THROW( {
    acquisition->set_channel_queue(&channel_internal_queue);
	}) << "Failure setting channel_internal_queue."<< std::endl;

	ASSERT_NO_THROW( {
	acquisition->set_threshold(config->property("Acquisition.threshold", 0.0));
	}) << "Failure setting threshold."<< std::endl;

	ASSERT_NO_THROW( {
	acquisition->set_doppler_max(config->property("Acquisition.doppler_max",
            10000));
	}) << "Failure setting doppler_max."<< std::endl;

	ASSERT_NO_THROW( {
	acquisition->set_doppler_step(config->property("Acquisition.doppler_step",
            500));
	}) << "Failure setting doppler_step."<< std::endl;

	ASSERT_NO_THROW( {
			acquisition->connect(top_block);
	}) << "Failure connecting acquisition to the top_block."<< std::endl;

	ASSERT_NO_THROW( {
	        std::string file = "../src/tests/signal_samples/GSoC_CTTC_capture_2012_07_26_4Msps_4ms.dat";
	        const char * file_name = file.c_str();
			gr_file_source_sptr file_source = gr_make_file_source(sizeof(gr_complex),file_name,false);

            top_block->connect(file_source, 0, acquisition->get_left_block(), 0);
	}) << "Failure connecting the blocks of acquisition test."<< std::endl;

	start_queue();
	acquisition->init();
	acquisition->reset();

	EXPECT_NO_THROW( {
			gettimeofday(&tv, NULL);
			begin = tv.tv_sec *1000000 + tv.tv_usec;
			top_block->run(); // Start threads and wait
			gettimeofday(&tv, NULL);
			end = tv.tv_sec *1000000 + tv.tv_usec;
		}) << "Failure running he top_block."<< std::endl;

	ch_thread.join();

	unsigned long int nsamples = gnss_synchro.Acq_samplestamp_samples;
	std::cout <<  "Acquired " << nsamples << " samples in " << (end-begin) << " microseconds" << std::endl;

	EXPECT_EQ(1, message) << "Acquisition failure. Expected message: 1=ACQ SUCCESS.";

	delete acquisition;

}