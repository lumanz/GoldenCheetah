/*
 * Copyright (c) 2011 Damien Grauser
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "GoldenCheetah.h"
#include "HrPwWindow.h"
#include "MainWindow.h"
#include "LTMWindow.h"
#include "HrPwPlot.h"
#include "SmallPlot.h"
#include "RideItem.h"
#include <math.h>
#include <stdlib.h>
#include <QVector>
#include <QtGui>

// tooltip

HrPwWindow::HrPwWindow(MainWindow *mainWindow) :
     GcWindow(mainWindow), mainWindow(mainWindow), current(NULL)
{
    setControls(NULL);
    setInstanceName("HrPw");

	QVBoxLayout *vlayout = new QVBoxLayout;
    setLayout(vlayout);

    // just the hr and power as a plot
    smallPlot = new SmallPlot(this);
    smallPlot->setMaximumHeight(200);
    smallPlot->setMinimumHeight(100);
    vlayout->addWidget(smallPlot);
    vlayout->setStretch(0, 20);

    // main plot
    hrPwPlot = new HrPwPlot(mainWindow, this);


    // tooltip on hover over point
    hrPwPlot->tooltip = new LTMToolTip(QwtPlot::xBottom, QwtPlot::yLeft,
                               QwtPicker::PointSelection,
                               QwtPicker::VLineRubberBand,
                               QwtPicker::AlwaysOn,
                               hrPwPlot->canvas(),
                               "");

    hrPwPlot->tooltip->setSelectionFlags(QwtPicker::PointSelection | QwtPicker::RectSelection
                                         | QwtPicker::DragSelection);
    hrPwPlot->tooltip->setRubberBand(QwtPicker::VLineRubberBand);
    hrPwPlot->tooltip->setMousePattern(QwtEventPattern::MouseSelect1,
                                         Qt::LeftButton, Qt::ShiftModifier);
    hrPwPlot->tooltip->setTrackerPen(QColor(Qt::black));

    hrPwPlot->_canvasPicker = new LTMCanvasPicker(hrPwPlot);

    // setup the plot
    QColor inv(Qt::white);
    inv.setAlpha(0);
    hrPwPlot->tooltip->setRubberBandPen(inv);
    hrPwPlot->tooltip->setEnabled(true);
    vlayout->addWidget(hrPwPlot);
    vlayout->setStretch(1, 100);

    // the controls
    QWidget *c = new QWidget(this);
    setControls(c);
    QFormLayout *cl = new QFormLayout(c);
    QLabel *delayLabel = new QLabel(tr("HR delay"), this);
    delayEdit = new QLineEdit(this);
    delayEdit->setFixedWidth(30);
    cl->addRow(delayLabel, delayEdit);
    delaySlider = new QSlider(Qt::Horizontal);
    delaySlider->setTickPosition(QSlider::TicksBelow);
    delaySlider->setTickInterval(1);
    delaySlider->setMinimum(1);
    delaySlider->setMaximum(100);;
    delayEdit->setValidator(new QIntValidator(delaySlider->minimum(),
                                              delaySlider->maximum(),
                                              delayEdit));
    cl->addRow(delaySlider);

    joinlineCheckBox = new QCheckBox(this);;
    joinlineCheckBox->setText(tr("Join points"));
    joinlineCheckBox->setCheckState(Qt::Unchecked);
    setJoinLineFromCheckBox();
    cl->addRow(joinlineCheckBox);

    shadeZones = new QCheckBox(this);;
    shadeZones->setText(tr("Shade Zones"));
    shadeZones->setCheckState(Qt::Checked);
    setShadeZones();
    cl->addRow(shadeZones);

    // connect them all up now initialised
    connect(hrPwPlot->_canvasPicker, SIGNAL(pointHover(QwtPlotCurve*, int)),
                            hrPwPlot, SLOT(pointHover(QwtPlotCurve*, int)));
    connect(joinlineCheckBox, SIGNAL(stateChanged(int)), this, SLOT(setJoinLineFromCheckBox()));
    connect(shadeZones, SIGNAL(stateChanged(int)), this, SLOT(setShadeZones()));
    connect(delayEdit, SIGNAL(editingFinished()), this, SLOT(setSmoothingFromLineEdit()));
    //connect(mainWindow, SIGNAL(configChanged()), this, SLOT(configChanged()));
    connect(this, SIGNAL(rideItemChanged(RideItem*)), this, SLOT(rideSelected()));
}

void
HrPwWindow::rideSelected()
{
    if (!amVisible())
        return;

    RideItem *ride = myRideItem;
    if (!ride || !ride->ride()) return;

    setData(ride);
}

void
HrPwWindow::setData(RideItem *ride)
{
    hrPwPlot->setDataFromRide(ride);
    smallPlot->setData(ride);
}

void
HrPwWindow::setJoinLineFromCheckBox()
{
    if (hrPwPlot->joinLine != joinlineCheckBox->isChecked()) {
        hrPwPlot->setJoinLine(joinlineCheckBox->isChecked());
        hrPwPlot->replot();
    }
}

void
HrPwWindow::setShadeZones()
{
    if (hrPwPlot->shadeZones != shadeZones->isChecked()) {
        hrPwPlot->setShadeZones(shadeZones->isChecked());
        hrPwPlot->replot();
    }
}

void
HrPwWindow::setSmoothingFromLineEdit()
{
    int value = delayEdit->text().toInt();
        //if (value != allPlot->smoothing()) {
        //allPlot->setSmoothing(value);
        delaySlider->setValue(value);
    //}
}

int
HrPwWindow::findDelay(QVector<double> &wattsArray, QVector<double> &hrArray, int rideTimeSecs)
{
    int delay = 0;
    double maxr = 0;

    for (int a = 10; a <=60; ++a) {

        QVector<double> delayHr(rideTimeSecs);

        for (int j = a; j<rideTimeSecs; ++j) {
            delayHr[j-a] = hrArray[j];
        }
        for (int j = rideTimeSecs-a; j<rideTimeSecs; ++j) {
            delayHr[j] = 0.0;
        }

        double r = corr(wattsArray, delayHr, rideTimeSecs-a);
        //fprintf(stderr, "findDelay %d: %.2f \n", a, r);


        if (r>maxr) {
            maxr = r;
            delay = a;
        }
    }
    delayEdit->setText(QString("%1").arg(delay));
    delaySlider->setValue(delay);
    return delay;
}

/**************************************/
/* Fichier CPP de la librairie reglin */
/* Réalisé par GONNELLA Stéphane      */
/**************************************/


/* Déclaratin globale des variables */


/*********************/
/* Fonctions de test */
/*********************/

/* Fonction de test de présence d'un zéro */

int HrPwWindow::test_zero(QVector<double> &tab,int n)
{
        for(int i=0;i<n;i++)
        {
                if(tab[i]==0)
                { return i;}
        }

        return -1;
}

/* Fonction de test de présence d'un négatif */

int HrPwWindow::test_negatif(QVector<double> &tab,int n)
{
        for(int i=0;i<n;i++)
        {
                if(tab[i]<0)
                { return i;}
        }

        return -1;
}

/*********************************/
/* Fonctions de valeurs absolues */
/*********************************/

/*fonction qui retourne la valeur absolue*/

double HrPwWindow::val_abs(double x)
{
	if(x<0) {return -x;}
	else {return x;}
}
/********************************/
/* Fonction de recherche du max */
/********************************/

/* Fonction qui retourne celui qui est le max */

int HrPwWindow::rmax(QVector<double> &r)
{
        double temp=0;
        int ajust=0;

        for(int i=0;i<5;i++)
        {
                if(r[i]>temp)
                {
                     temp=r[i];
                     ajust = i+1;
                }
        }

        return ajust;
}

/**********************/
/* Fonctions de somme */
/**********************/

/* Fonction de somme d'éléments d'un tableau d'entier */

int HrPwWindow::somme(QVector<int> &tab,int n)
{
	int somme=0;

	for (int i=0;i<n;i++)
	{
	 somme=((tab[i])+(somme));
   	}

	return(somme);
}

/* Fonction de somme d'éléments d'un tableau de réel*/

double HrPwWindow::somme(QVector<double> &tab,int n)
{
	double somme=0;

	for (int i=0;i<n;i++)
	{
	 somme=((tab[i])+(somme));
   	}

	return(somme);
}

/**********************************/
/* Fonctions de calcul de moyenne */
/**********************************/

/* Fonction de calcul de moyenne d'éléments d'un tableau d'entier */

double HrPwWindow::moyenne(QVector<int> &tab,int n)
{
	double moyenne = double(somme(tab,n))/n;

	return (moyenne);
}

/* Fonction de calcul de moyenne d'éléments d'un tableau de réel */

double HrPwWindow::moyenne(QVector<double> &tab,int n)
{
	double moyenne = somme(tab,n)/n;

	return (moyenne);
}

/* Fonction de calcul de moyenne d'elements d'un tableau de réel(2) */

double HrPwWindow::moyenne2(double somme,int n)
{
	double moyenne = somme/n;

	return (moyenne);
}

/***********************/
/* Fonction de produit */
/***********************/

/* Fonction de calcul du produit d'éléments de deux tableau ligne par ligne */

void HrPwWindow::produittab(QVector<double> &tab1,QVector<double> &tab2,int n)
{
    tab_temp.resize(n);   //Réservation de l'espace mémoire

	for (int i=0;i<n;i++)
	{
		tab_temp[i]=(tab1[i]*tab2[i]);
	}
}

/***************************************/
/* Fonctions de changement de variable */
/***************************************/

/* Fonctions de calcul du ln des éléments d'un tableau de réel */

void HrPwWindow::lntab(QVector<double> &tab,QVector<double> &tabTemp,int n)
{
    tab_temp.resize(n);   //Réservation de l'espace mémoire

	for (int i=0;i<n;i++)
	{
		tabTemp[i]=(log(tab[i]));
	}
}

/* Fonctions de calcul du log base 10 des éléments d'un tableau de réel */

void HrPwWindow::logtab(QVector<double> &tab,QVector<double> &tabTemp,int n)
{
    tab_temp.resize(n);   //Réservation de l'espace mémoire

	for (int i=0;i<n;i++)
	{
		tabTemp[i]=(log10(tab[i]));
	}
}

/* Fonction d'inverstion des élements d'un tableau de réel */

void HrPwWindow::invtab(QVector<double> &tab,QVector<double> &tabTemp,int n)
{
    tab_temp.resize(n);   //Réservation de l'espace mémoire

	for (int i=0;i<n;i++)
	{
		tabTemp[i]=(1/tab[i]);
	}
}

/********************/
/* Autres fonctions */
/********************/

/* Fonction de calcul des écarts à la moyenne */

void HrPwWindow::ecart_a_moyenne(QVector<double> &tab,double Moyenne,int n)
{
    tab_temp.resize(n);   //Réservation de l'espace mémoire

	for (int i=0;i<n;i++)
	{
		tab_temp[i]=(tab[i] - Moyenne);
	}
}

/****************************/
/* Fonctions de statistique */
/****************************/

/* Fonction de calcul de la covariance */

double HrPwWindow::covariance(QVector<double> &Xi, QVector<double> &Yi,int n)
{
	double cov;

	produittab(Xi,Yi,n);
	cov = moyenne(tab_temp,n) - ( moyenne(Xi,n) * moyenne(Yi,n) );

	return (cov);
}

/* Fonction de calcul de la somme des carrés des écarts a la moyenne */

double HrPwWindow::variance(QVector<double> &val,int n)
{
	double sce;

	produittab(val,val,n);
	sce = moyenne(tab_temp,n) - ( moyenne(val,n) * moyenne(val,n));

  	return (sce);
}

/* Fonction de calcul de l'écart-type */

double HrPwWindow::ecarttype(QVector<double> &val,int n)
{
	double ect= sqrt(variance(val,n));

	return (ect);
}
/******************************************************/
/* Fonctions pour le calcul de la régression linéaire */
/* par la méthode des moindres carré                  */
/******************************************************/

/* Fonction de clacul de la pente (a) */

double HrPwWindow::pente(QVector<double> &Xi,QVector<double> &Yi,int n)
{
	double a = covariance(Xi,Yi,n)/variance(Xi,n);

	return (a);
}

/* Fonction de clacul de l'ordonnée a l'origine (b) */

double HrPwWindow::ordonnee(QVector<double> &Xi,QVector<double> &Yi,int n)
{
	double b = moyenne(Yi,n) - ( pente(Xi,Yi,n) * moyenne(Xi,n) );

	return (b);
}

/* Fonction de calcul du coef de corrélation (r) */

double HrPwWindow::corr(QVector<double> &Xi, QVector<double> &Yi,int n)
{
	double r = covariance(Xi,Yi,n)/(ecarttype(Xi,n)*ecarttype(Yi,n));
        //double r=pente(Xi,Yi,n)*pente(Xi,Yi,n)*(variance(Xi,n)/variance(Yi,n));
	return (r);
}

/* Fonction de détermination du meilleur ajustement */

int HrPwWindow::ajustement(QVector<double> &Xi,QVector<double> &Yi,int n)
{
        QVector<double> r(5),lnXi(100),lnYi(100),logXi(100),logYi(100),invXi(100);

        //corrélation pour linéaire

        r[0]=val_abs(corr(Xi,Yi,n));

        //corrélation pour exponetielle

        lntab(Yi,lnYi,n);
        r[1]=val_abs(corr(Xi,lnYi,n));

        //corrélation pour puissance

        logtab(Xi,logXi,n);
        logtab(Yi,logYi,n);
        r[2]=val_abs(corr(logXi,logYi,n));

        //corrélation pour inverse

        invtab(Xi,invXi,n);
        r[3]=val_abs(corr(invXi,Yi,n));

        //corrélation pour logarithmique

        lntab(Xi,lnXi,n);
        r[4]=val_abs(corr(lnXi,Yi,n));

        //Test du meilleur ajustement

        return rmax(r);
}

/*****************************/
/* Fin du fichier reglin.cpp */
/*****************************/
